import string
from typing import Tuple, Optional
import warnings

import pandas as pd


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
    def __init__(self, well, id: str, rel_pos: Tuple[float, float], index=None):
        self._well = well
        self._id = id
        self._rel_pos = rel_pos
        self._index = index

    def __repr__(self):
        if self._index:
            return "Site(well='{}', id='{}', index={}, rel_pos={})".format(self._well.id, self._id, self._index, self._rel_pos)
        else:
            return "Site(well='{}', id='{}', rel_pos={})".format(self._well.id, self._id, self._rel_pos)

    @property
    def well_id(self) -> str:
        return self._well.id

    @property
    def id(self) -> str:
        return self._id

    @property
    def index(self):
        return self.index

    @property
    def rel_pos(self) -> Tuple[float, float]:
        return self._rel_pos

    @property
    def pos(self) -> Tuple[float, float]:
        pos_x = self._well.pos_origin[0] + self._rel_pos[0]
        pos_y = self._well.pos_origin[1] + self._rel_pos[1]
        return (pos_x, pos_y)


class Well():
    def __init__(self, id: str):
        self._id = id
        self._enabled = False
        self._preset_name = None
        self._pos_origin = None
        self._sites = []
        self.metadata = {}

    @property
    def id(self) -> str:
        return self._id

    def __repr__(self):
        return "Well('{}')".format(self._id)

    def enable(self):
        self._enabled = True

    def disable(self):
        self._enabled = False

    def is_enabled(self) -> bool:
        return self._enabled

    @property
    def pos_origin(self) -> Tuple[float, float]:
        if self._pos_origin is None:
            raise ValueError("well origin is not set up")

        return self._pos_origin

    @pos_origin.setter
    def pos_origin(self, pos:  Tuple[float, float]):
        # TODO: check safety range
        self._pos_origin = pos

    @property
    def preset_name(self) -> str:
        return self._preset_name

    @preset_name.setter
    def preset_name(self, preset_name: str):
        self._preset_name = preset_name

    def setup_sites(self, n_x: int, n_y: int, n_sites: Optional[int]=None, spacing: float =-250):
        sites = []
        for i_y in range(n_y):
            i_x_list = list(range(n_x))
            if i_y % 2 != 0:
                i_x_list = i_x_list[::-1]
            for i_x in i_x_list:
                site_id = "{:03d}".format(len(sites))
                site_index = (i_x, i_y)
                site_rel_pos = (i_x * spacing, i_y * spacing)
                sites.append(Site(self, site_id, site_rel_pos, site_index))
        
        if n_sites:
            self._sites = sites[:n_sites]
        else:
            self._sites = sites

    @property
    def sites(self):
        return self._sites


class Wellplate():
    def __init__(self, type: str, id: str):
        self._id = id

        if type not in WELLPLATE_CONFIG:
            raise ValueError("unknown type {}".format(type))
        self._type = type

        wp_config = WELLPLATE_CONFIG[type]
        self._spacing = wp_config["spacing"]
        self._wellsize = wp_config["wellsize"]
        self._n_rows = wp_config["n_rows"]
        self._n_cols = wp_config["n_cols"]

        self._n_wells = self.n_rows * self.n_cols
        self._rows = string.ascii_uppercase[:self.n_rows]
        self._cols = ["{:02d}".format(i_col) for i_col in range(1, self.n_cols+1)]

        self._wells = {}

        for row_id in self._rows:
            for col_id in self._cols:
                well_id = row_id + col_id
                self._wells[well_id] = Well(well_id)

        self._pos_origin = None
    
    def __repr__(self):
        return "Wellplate(id='{}', type='{}')".format(self._id, self._type)

    @property
    def id(self) -> str:
        return self._id

    @property
    def type(self) -> str:
        return self._type

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
        self._pos_origin = pos
        for i_col in range(self._n_cols):
            for i_row in range(self._n_rows):
                well_pos_x = pos[0] + self._spacing[0] * i_col
                well_pos_y = pos[1] + self._spacing[1] * i_row
                well_id = self.rowcol_to_id(i_row, i_col)
                self._wells[well_id].pos_origin = (well_pos_x, well_pos_y)

    def define_origin(self, current_well_id: str, pos=None):
        if pos is None:
            pass
        i_row, i_col = self.id_to_rowcol(current_well_id)
        pos_origin_x = pos[0] + self._spacing[0] * i_col
        pos_origin_y = pos[1] + self._spacing[1] * i_row
        self.pos_origin = (pos_origin_x, pos_origin_y)

    def enabled_wells(self):
        selected_well_ids = []
        for well_id in self._wells:
            if self._wells[well_id].is_enabled():
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
        df_columns = ["well", "row", "col", "site", "pos_x", "pos_y", "preset_name"]

        well_missing_preset = set()
        for well_id in well_ids_routed:
            well = self._wells[well_id]
            row = well_id[0]
            col = int(well_id[1:])
            for site in well.sites:
                if not well.preset_name:
                    well_missing_preset.add(well_id)
                if self._pos_origin is None:
                    df_row = [well_id, row, col, site.id, float('nan'), float('nan'), well.preset_name or '']
                else:
                    df_row = [well_id, row, col, site.id, site.pos[0], site.pos[1], well.preset_name or '']
                
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
        for well_id in self._well_ids:
            self._wellplate[well_id].preset_name = preset_name

    def enable(self):
        for well_id in self._well_ids:
            self._wellplate[well_id].enable()

    def disable(self):
        for well_id in self._well_ids:
            self._wellplate[well_id].disable()

    def setup_sites(self, *args):
        for well_id in self._well_ids:
            self._wellplate[well_id].setup_sites(*args)

    @property
    def metadata(self):
        return WellplateSliceMetadata(self)


class WellplateSliceMetadata():
    def __init__(self, wellplate_slice):
        self._wellplate_slice = wellplate_slice

    def __setitem__(self, key, value):
        for well in self._wellplate_slice.wells():
            well.metadata[key] = value
