from cgitb import enable
from re import X
import string
import warnings
import json

import pandas as pd

from typing import Tuple, Optional

from . import api_pb2

WELLPLATE_CONFIG = {
    "wellplate96": {
        "model": "Brooks MGB096-1-2-LG-L",
        "spacing": (-9000, -9000),
        "wellsize": (7520, 7520),
        "n_rows": 8,
        "n_cols": 12,
    },
    "wellplate384": {
        "model": "Brooks MGB101-1-2-LG-L",
        "spacing": (-4500, -4500),
        "wellsize": (2900, 2900),
        "n_rows": 16,
        "n_cols": 24,
    }
}


class Site():
    def __init__(self, well, id: str, rel_pos: Tuple[float, float]):
        self._well = well
        self._uuid = ""
        self._id = id
        self._rel_pos = rel_pos
        self._metadata = {}

    def __repr__(self):
        if self.name:
            return "Site(well='{}', id='{}', name={}, rel_pos={})".format(self._well.id, self._id, self.name, self._rel_pos)
        else:
            return "Site(well='{}', id='{}', rel_pos={})".format(self._well.id, self._id, self._rel_pos)

    @property
    def well_id(self) -> str:
        return self._well.id
    
    @property
    def uuid(self):
        return self._uuid

    @property
    def id(self) -> str:
        return self._id

    @property
    def name(self):
        if "name" in self._metadata:
            return self._metadata["name"]
        else:
            return None

    @property
    def rel_pos(self) -> Tuple[float, float]:
        return self._rel_pos

    @property
    def pos(self) -> Tuple[float, float]:
        pos_x = self._well.pos[0] + self._rel_pos[0]
        pos_y = self._well.pos[1] + self._rel_pos[1]
        return (pos_x, pos_y)

class Well():
    def __init__(self, wellplate, id: str):
        self._wellplate = wellplate
        self._uuid = ""
        self._id = id
        self._enabled = False
        self._rel_pos = None
        self._sites = []
        self.metadata = {}

    @property
    def uuid(self):
        return self._uuid
    
    @property
    def id(self) -> str:
        return self._id

    def __repr__(self):
        return "Well('{}')".format(self._id)

    def set_enabled(self):
        req_list = []
        req = api_pb2.ModifyWellRequest(
                well_uuid = self.uuid,
                enable = True)
        req_list.append(req)
        self._wellplate._api.stub.ModifyWell(req for req in req_list)

    def set_disabled(self):
        req_list = []
        req = api_pb2.ModifyWellRequest(
                well_uuid = self.uuid,
                enable = False)
        req_list.append(req)
        self._wellplate._api.stub.ModifyWell(req for req in req_list)

    def enabled(self) -> bool:
        return self._enabled

    @property
    def pos(self) -> Tuple[float, float]:
        pos_x = self._wellplate.pos_origin[0] + self._rel_pos[0]
        pos_y = self._wellplate.pos_origin[1] + self._rel_pos[1]
        return (pos_x, pos_y)

    @property
    def rel_pos(self) -> Tuple[float, float]:
        if self._rel_pos is None:
            raise ValueError("well position is not set up")

        return self._rel_pos

    @property
    def preset_name(self) -> str:
        if "preset_name" in self._metadata:
            return self._metadata["preset_name"]
        return None

    @preset_name.setter
    def preset_name(self, preset_name: str):
        raise NotImplementedError()

    def create_sites(self, n_x: int, n_y: int, spacing: float =-250):
        raise NotImplementedError()

    @property
    def sites(self):
        return self._sites


class Plate():
    def __init__(self, type: str, id: str, api=None):
        self._uuid = ""
        self._type = type
        self._id = id
        self._pos_origin = None
        self._metadata = {}
        self._wells = {}
        self._api = api
        
        if type not in WELLPLATE_CONFIG:
            raise ValueError("unknown type {}".format(type))

        wp_config = WELLPLATE_CONFIG[type]
        self._spacing = wp_config["spacing"]
        self._wellsize = wp_config["wellsize"]
        self._n_rows = wp_config["n_rows"]
        self._n_cols = wp_config["n_cols"]

        self._n_wells = self.n_rows * self.n_cols
        self._rows = string.ascii_uppercase[:self.n_rows]
        self._cols = ["{:02d}".format(i_col) for i_col in range(1, self.n_cols+1)]
    
    def __repr__(self):
        return "Wellplate(type='{}', id='{}')".format(self._id, self._type)

    @property
    def uuid(self):
        return self._uuid
    
    @property
    def id(self) -> str:
        return self._id

    @property
    def type(self) -> str:
        return self._type

    @property
    def name(self):
        if "name" in self._metadata:
            return self._metadata["name"]
        else:
            return None

    @name.setter
    def name(self, name: str):
        req = api_pb2.SetPlateMetadataRequest(
                plate_uuid = self.uuid,
                key = "name",
                json_value = json.dumps(name))
        self._api.stub.SetPlateMetadata(req)

    @property
    def n_rows(self) -> int:
        return self._n_rows

    @property
    def n_cols(self) -> int:
        return self._n_cols

    @property
    def n_wells(self) -> int:
        return self._n_wells

    def rowcol_to_id(self, i_row: int, i_col: int) -> str:
        row_id = self._rows[i_row]
        col_id = self._cols[i_col]
        return row_id + col_id

    def id_to_rowcol(self, well_id: str) -> Tuple[int, int]:
        row_id = well_id[0]
        col_id = well_id[1:]

        i_row = self._rows.index(row_id)
        i_col = int(col_id) - 1
        if i_col < 0 or i_col >= self._n_cols:
            raise ValueError("invalid well id")
        return i_row, i_col

    def is_id_valid(self, well_id: str):
        row_id = well_id[0]
        col_id = well_id[1:]
        i_col = int(col_id) - 1

        if not row_id in self._rows:
            return False
        if i_col < 0 or i_col >= self._n_cols:
            return False
        return True

    def normalize_id(self, well_id: str) -> str:
        return self.rowcol_to_id(*self.id_to_rowcol(well_id))

    @property
    def pos_origin(self) -> Tuple[float, float]:
        return self._pos_origin

    @pos_origin.setter
    def pos_origin(self, pos: Tuple[float, float]):
        # TODO check against safety range
        req = api_pb2.SetPlatePositionOriginRequest(
                plate_uuid = self.uuid,
                x = pos[0],
                y = pos[1])
        self._api.stub.SetPlatePositionOrigin(req)

    def define_origin(self, current_well_id: str, pos:Optional[Tuple[float, float]]=None):
        if pos is None:
            pos = self._api.get_xy_stage_position()
        
        rel_pos = self._wells[self.normalize_id(current_well_id)].rel_pos
        pos_origin_x = pos[0] - rel_pos[0]
        pos_origin_y = pos[1] - rel_pos[1]
        self.pos_origin = (pos_origin_x, pos_origin_y)

    def move_to_position(self, well_id: str, i_x: int, i_y: int, grid_spacing=-250, wait=True):
        # TODO validate i_x, i_y
        well = self._wells[well_id]
        well_pos_x, well_pos_y = well.pos
        pos_x = well_pos_x + i_x * grid_spacing
        pos_y = well_pos_y + i_y * grid_spacing
        self._api.set_xy_stage_position(pos_x, pos_y)
        if wait:
            self._api.wait_xy_stage()
    
    def enabled_wells(self):
        selected_well_ids = []
        for well_id in self._wells:
            if self._wells[well_id].enabled():
                selected_well_ids.append(well_id)
        return WellplateSlice(self, selected_well_ids)

    def __getitem__(self, index):
        # ["A3"]
        if isinstance(index, str):
            well_id = self.normalize_id(index)
            return self._wells[well_id]
        
        # [["A1", "H1"]]
        if isinstance(index, list):
            well_ids = [self.normalize_id(id) for id in index]
            return WellplateSlice(self, well_ids)
        
        # ["A1":"H10"] or ["A1":"A8":2]
        if isinstance(index, slice):
            i_row_start, i_col_start = self.id_to_rowcol(index.start)
            i_row_stop, i_col_stop = self.id_to_rowcol(index.stop)
            selected_well_ids = []
            
            if index.step:
                if not (isinstance(index.step, int) and index.step > 0):
                    raise ValueError("invalid step")
                if i_row_start == i_row_stop:
                    for i_col in range(i_col_start, i_col_stop+1, index.step):
                        selected_well_ids.append(self.rowcol_to_id(i_row_start, i_col))
                elif i_col_start == i_col_stop:
                    for i_row in range(i_row_start, i_row_stop+1, index.step):
                        selected_well_ids.append(self.rowcol_to_id(i_row, i_col_start))
                else:
                    raise ValueError("invalid index")
            else:
                for i_row in range(i_row_start, i_row_stop+1):
                    for i_col in range(i_col_start, i_col_stop+1):
                        selected_well_ids.append(self.rowcol_to_id(i_row, i_col))
            
            return WellplateSlice(self, selected_well_ids)
        
        # row and col are selected seperately
        #
        # i.e.:
        # ["A":"H", 1:10:2], ["A":"H", 1], ["A":"H", [1,4,5]]
        # ["A", 1:8], [["A", "C"], 1:8:2]
        # etc.
        if isinstance(index, tuple) and len(index) == 2:
            index_row, index_col = index
            
            selected_rows = []
            if isinstance(index_row, str):
                if index_row not in self._rows:
                    raise ValueError("invalid row")
                
                selected_rows = [index_row]
                    
            elif isinstance(index_row, list):
                for row_id in index_row:
                    if row_id not in self._rows:
                        raise ValueError("invalid row")
                
                selected_rows = index_row
                
            elif isinstance(index_row, slice):
                i_row_start = self._rows.index(index_row.start)
                i_row_stop = self._rows.index(index_row.stop)
                i_row_step = 1
                if index_row.step:
                    if not (isinstance(index_row.step, int) and index_row.step > 0):
                        raise ValueError("invalid step")
                    i_row_step = index_row.step
                selected_rows = self._rows[i_row_start:(i_row_stop+1):i_row_step]
            
            else:
                raise ValueError("unsupported index: {}".format(index))
            
            selected_cols = []
            if isinstance(index_col, int):
                col_id = "{:02d}".format(index_col)
                if col_id not in self._cols:
                    raise ValueError("invalid col")
                else:
                    selected_cols = [col_id]
                        
            elif isinstance(index_col, list):
                for index_col_item in index_col:
                    col_id = None
                    if isinstance(index_col_item, int):
                        col_id = str(index_col_item)
                    else:
                        raise ValueError("invalid col")
                    
                    if col_id not in self._cols:
                        raise ValueError("invalid col")
                    else:
                        selected_cols.append(col_id)
                        
            elif isinstance(index_col, slice):
                i_col_start = int(str(index_col.start)) - 1
                i_col_stop = int(str(index_col.stop)) - 1
                if i_col_start < 0 or i_col_start >= self._n_cols:
                    raise ValueError("invalid col")
                if i_col_stop < 0 or i_col_stop >= self._n_cols:
                    raise ValueError("invalid col")
                i_col_step = 1
                if index_col.step:
                    if not (isinstance(index_col.step, int) and index_col.step > 0):
                        raise ValueError("invalid step")
                    i_col_step = index_col.step
                selected_cols = self._cols[i_col_start:(i_col_stop+1):i_col_step]
            else:
                raise ValueError("unsupported index: {}".format(index))
            
            selected_well_ids = []
            for row_id in selected_rows:
                for col_id in selected_cols:
                    well_id = row_id+col_id
                    selected_well_ids.append(well_id)
            return WellplateSlice(self, selected_well_ids)
        
        raise ValueError("unsupported index: {}".format(index))

    def df_sites(self):        
        # ------ find all metadata keys
        metadata_columns = []
        for well in self.enabled_wells().wells():
            for k in well.metadata:
                if k not in metadata_columns:
                    metadata_columns.append(k)
                    
        # ------ sort wells in snape order
        rows = []
        well_ids_of_row = {}
        for id in sorted(self.enabled_wells().well_ids()):
            row = id[0]
            if row in rows:
                well_ids_of_row[row].append(id)
            else:
                rows.append(row)
                well_ids_of_row[row] = [id]

        well_ids_routed = []
        for i, row in enumerate(rows):
            if i % 2 == 0:
                well_ids_routed += well_ids_of_row[row]
            else:
                well_ids_routed += well_ids_of_row[row][::-1]
        
        # ------ generate the dataframe 
        df = []
        df_columns = ["site_uuid", "well", "row", "col", "site", "pos_x", "pos_y", "preset_name"]

        well_missing_preset = set()
        for well_id in well_ids_routed:
            well = self._wells[well_id]
            row = well_id[0]
            col = int(well_id[1:])
            for site in well.sites:
                if not well.preset_name:
                    well_missing_preset.add(well_id)
                if self._pos_origin is None:
                    df_row = [site.uuid, well_id, row, col, site.id, float('nan'), float('nan'), well.preset_name or '']
                else:
                    df_row = [site.uuid, well_id, row, col, site.id, site.pos[0], site.pos[1], well.preset_name or '']
                
                for k in metadata_columns:
                    if k in well.metadata:
                        df_row.append(well.metadata[k])
                    else:
                        df_row.append(None)
                
                df.append(df_row)
        df = pd.DataFrame(df, columns=df_columns+metadata_columns)
        
        # ------ warn about missing origin
        if not self._pos_origin:
            warnings.warn("origin is not set up")
        
        # ------ warn about missing presets
        if len(well_missing_preset) > 0:
            warnings.warn("wells without preset: {}".format(sorted(list(well_missing_preset))))
        
        return df


class WellplateSlice():
    def __init__(self, wellplate, well_ids):
        self._wellplate = wellplate
        self._well_ids = well_ids

    def __len__(self):
        return len(self._well_ids)

    def __repr__(self):
        return "WellplateSlice(plate_id='{}', wells={})".format(self._wellplate.id, self._well_ids)

    def well_ids(self):
        return self._well_ids

    def wells(self):
        return [self._wellplate[well_id] for well_id in self._well_ids]

    @property
    def preset_name(self):
        ch_well_map = {}
        for well_id in self._well_ids:
            ch = self._wellplate[well_id].preset_name
            if not ch in ch_well_map:
                ch_well_map[ch] = []
            ch_well_map[ch].append(well_id)
        return ch_well_map

    @preset_name.setter
    def preset_name(self, preset_name):
        req = api_pb2.SetWellsMetadataRequest(
                plate_uuid = self._wellplate.uuid,
                well_id = self._well_ids,
                key = "preset_name",
                json_value = json.dumps(preset_name))
        self._wellplate._api.stub.SetWellsMetadata(req)

    def enable(self):
        req = api_pb2.SetWellsEnabledRequest(
                plate_uuid = self._wellplate.uuid,
                well_id = self._well_ids,
                enabled = True)
        self._wellplate._api.stub.SetWellsEnabled(req)

    def disable(self):
        req = api_pb2.SetWellsEnabledRequest(
                plate_uuid = self._wellplate.uuid,
                well_id = self._well_ids,
                enabled = False)
        self._wellplate._api.stub.SetWellsEnabled(req)

    def create_sites(self, n_x: int, n_y: int, spacing: float = -250):
        req = api_pb2.CreateSitesRequest(
                plate_uuid = self._wellplate.uuid,
                well_id = self._well_ids,
                n_x = n_x,
                n_y = n_y,
                spacing_x = spacing,
                spacing_y = spacing)
        self._wellplate._api.stub.CreateSites(req)

    @property
    def metadata(self):
        return WellplateSliceMetadata(self)


class WellplateSliceMetadata():
    def __init__(self, wellplate_slice):
        self._wellplate_slice = wellplate_slice

    def __setitem__(self, key, value):
        for well in self._wellplate_slice.wells():
            well.metadata[key] = value
