import threading
from IPython.display import display
import ipywidgets as widgets
import time
import math
import datetime
import numpy as np

from .api import API, Channel

class MultiSiteTaskStatusWidget():
    def __init__(self, task):
        self._task = task
        
        self.sites_progress = widgets.IntProgress(value=0, min=0, max=1)
        self.sites_progress_label = widgets.Label("")
        sites_hbox = widgets.HBox([widgets.Label("Overall"), self.sites_progress, self.sites_progress_label])
        
        self.wells_progress = widgets.IntProgress(value=0, min=0, max=1)
        self.wells_progress_label = widgets.Label("")
        wells_hbox = widgets.HBox([widgets.Label("Wells"), self.wells_progress, self.wells_progress_label])
        
        self.button_stop = widgets.Button(description="Stop")
        self.button_stop.on_click(lambda btn: self._task.stop())
        self.button_stop.layout.visibility = "hidden"
        
        self.task_status_label = widgets.Label("")
        self.site_status_label = widgets.Label("")

        vbox = widgets.VBox([sites_hbox, wells_hbox, self.task_status_label, self.site_status_label, self.button_stop])
        display(vbox)
        
        self._status = "Not running"
        self._t_start = None
        self._t_stop = None
        self._t_remaining = None
        self.update_status_label()
        
    def update_status_label(self):
        msgs = []
        if self._t_start:
            msgs.append("Started at {}.".format(datetime.datetime.strftime(self._t_start, "%H:%M:%S")))
        
        if self._t_stop:
            msgs.append("Stopped at {}.".format(datetime.datetime.strftime(self._t_stop, "%H:%M:%S")))
            duration = self._t_stop - self._t_start
            duration = datetime.timedelta(seconds=int(round(duration.total_seconds())))
            msgs.append("Total time: {}".format(duration))
        else:
            msgs.append(self._status)
            if self._t_start:
                t_elapsed = datetime.datetime.now() - self._t_start
                t_elapsed = datetime.timedelta(seconds=int(round(t_elapsed.total_seconds())))
                msgs.append("Elapsed: {}".format(t_elapsed))
            
            if self._t_remaining:
                t_rounded = datetime.timedelta(seconds=int(round(self._t_remaining.total_seconds())))
                msgs.append("Remaining: {}".format(t_rounded))
        
        self.task_status_label.value = " ".join(msgs)
    
    def set_status(self, status):
        self._status = status
        self.update_status_label()
    
    def set_site_status(self, text):
        self.site_status_label.value = text
    
    def set_t_start(self, t_start):
        self._t_start = t_start
        self.update_status_label()
        self.button_stop.layout.visibility = "visible"
        self.update_status_label()
    
    def set_t_stop(self, t_stop):
        self._t_stop = t_stop
        self.update_status_label()
        self.button_stop.layout.visibility = "hidden"
        self.update_status_label()
    
    def set_t_remaining(self, t_remaining):
        self._t_remaining = t_remaining
        self.update_status_label()

    def set_sites_progress(self, n_sites, n_sites_total):
        self.sites_progress.max = n_sites_total
        self.sites_progress.value = n_sites
        self.sites_progress_label.value = "{}/{}".format(n_sites, n_sites_total)
        self.update_status_label()
    
    def set_wells_progress(self, n_wells, n_wells_total):
        self.wells_progress.max = n_wells_total
        self.wells_progress.value = n_wells
        self.wells_progress_label.value = "{}/{}".format(n_wells, n_wells_total)
        self.update_status_label()
    

# TODO: reimplement without using df_sites
class MultiSiteTask():
    def __init__(self, df_sites, presets: dict[str, Channel], api: API, seq_no=0, fn_after_acq=None, fn_before_acq=None):
        if df_sites["pos_x"].isnull().values.any():
            raise ValueError("contains missing positions")
        if df_sites["pos_y"].isnull().values.any():
            raise ValueError("contains missing positions")

        for preset_name in list(df_sites["preset_name"].unique()):
            if preset_name not in presets:
                raise ValueError("preset {} not found".format(preset_name))
        
        self._df_sites = df_sites
        self._presets = presets
        self._api = api
        self._seq_no = seq_no
        self._focus_task = FindFocusTask(self._api)
    
        self._t_start = None
        self._t_stop = None
        self.df_sites = None

        self.previous_well_id = None
        self.current_row = None
        self.fn_after_acq = fn_after_acq
        self.fn_before_acq = fn_before_acq

    def _run(self, find_focus=True):
        self._t_start = datetime.datetime.now()
        self.status_bar.set_t_start(self._t_start)
        self.status_bar.set_status("Running...")
        
        t = threading.current_thread()
        
        n_sites = self._df_sites.shape[0]
        n_wells = len(self._df_sites["well"].unique())

        self.previous_well_id = None
        wells_finished = set()

        for (i_site, row) in enumerate(self._df_sites.itertuples()):
            self.current_row = row
            
            # Determine progress
            well_id = row.well
            site_id = row.site
            preset_name = row.preset_name
            pos_x = row.pos_x
            pos_y = row.pos_y
            im_name = "{}-{}".format(well_id, site_id)
            
            if self.previous_well_id:
                if self.previous_well_id != well_id:
                    wells_finished.add(well_id)
            self.previous_well_id = well_id

            self.status_bar.set_sites_progress(i_site, n_sites)
            self.status_bar.set_wells_progress(len(wells_finished), n_wells)
            
            site_info_msg = "Well {}. Site {}. Preset: {}. Position: ({:.1f}, {:.1f})".format(well_id, site_id, preset_name, pos_x, pos_y)
            self.status_bar.set_site_status(site_info_msg)
            
            if i_site > 2:
                t_elapsed = datetime.datetime.now() - self._t_start
                n_sites_completed = i_site
                n_sites_remaining = n_sites - n_sites_completed
                t_remaining = t_elapsed / n_sites_completed * n_sites_remaining
                self.status_bar.set_t_remaining(t_remaining)

            # Check whether to preceed
            if getattr(t, "stop", False):
                site_info_msg += " Canceled"
                self.status_bar.set_site_status(site_info_msg)
                return
            
            ### Move to position
            self._api.set_xy_stage_position(pos_x, pos_y)
            self._api.wait_xy_stage()
            if find_focus:
                self._focus_task.find_focus(msg_prefix=im_name)
            
            if self.fn_before_acq:
                self.fn_before_acq(self)

            time.sleep(1)
            
            ### Acquire
            self._api.acquire_multi_channel(im_name, self._presets[preset_name], 0, self._seq_no, site_uuid=row.site_uuid)
            
            if self.fn_after_acq:
                self.fn_after_acq(self)

        self.status_bar.set_sites_progress(n_sites, n_sites)
        self.status_bar.set_wells_progress(len(wells_finished)+1, n_wells)
        self._t_stop = datetime.datetime.now()
        self.status_bar.set_t_stop(self._t_stop)

    def start(self, find_focus=True):        
        self.status_bar = MultiSiteTaskStatusWidget(self)
        self.thread = threading.Thread(target=MultiSiteTask._run, args=(self, find_focus))
        self.thread.start()

    def stop(self):
        self.thread.stop = True
        self.thread.join()
        
        self._t_stop = datetime.datetime.now()
        self.status_bar.set_t_stop(self._t_stop)
        self.status_bar.set_t_remaining(None)

class FindFocusTask():
    def __init__(self, api: API):
        self._api = api
    
    def find_focus(self, msg_prefix=""):
        try:
            self.enable_pfs()
            return
        except RuntimeError:
            pass

        z_initial = self._api.get_z_stage_position()
        z_max_safety = None

        try:
            print("{}: search focus within 30 um:".format(msg_prefix))
            self.search_focus_range(30, 5, z_initial, z_max_safety)
            print("focus found")
            return
        except RuntimeError:
            pass

        try:
            print("{}: search focus within 75 um:".format(msg_prefix))
            self.search_focus_range(75, 5, z_initial, z_max_safety)
            print("focus found")
            return
        except RuntimeError:
            pass

        try:
            print("{}: search focus within 150 um:".format(msg_prefix))
            self.search_focus_range(150, 5, z_initial, z_max_safety)
            print("focus found")
        except RuntimeError:
            print("{}: failed: go back to z={}".format(msg_prefix, z_initial))
            self._api.set_z_stage_position(z_initial)
            self._api.wait_z_stage()
            return
    
    def enable_pfs(self):
        while True:
            pfs_status = self._api.get_property("/NikonTi/PFSStatus")
            if pfs_status == "Locked in focus":
                return
            elif pfs_status == "Within range of focus search":
                self._api.set_property({"/NikonTi/PFSState": "On"})
                time.sleep(0.1)
                continue
            elif pfs_status == "Focusing":
                time.sleep(0.1)
                continue
            else:
                raise RuntimeError(pfs_status)

    def search_focus_range(self, z_range, z_step_size, z_center, z_max_safety=None):
        try:
            self.enable_pfs()
            return
        except:
            pass

        n_steps = math.ceil(z_range / z_step_size)
        z_min = z_center - (n_steps / 2) * z_step_size
        z_max = z_center + (n_steps / 2) * z_step_size
        if z_max_safety and (z_max > z_max_safety):
            z_max = z_max_safety
        if z_max_safety and (z_min > z_max_safety):
            raise ValueError(
                "z_min= {}, higher than z_max_safety ={}".format(z_min, z_max_safety))
        n_steps = math.ceil((z_max - z_min) / z_step_size)
        z_list = list(np.linspace(z_min, z_max, n_steps))
        z_list.reverse()
        for z_pos in z_list:
            print("set z={}".format(z_pos))
            self._api.set_z_stage_position(z_pos)
            self._api.wait_z_stage()
            try:
                self.enable_pfs()
                return
            except:
                pass

        raise RuntimeError("Focus search failed")
