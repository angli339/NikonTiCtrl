import numpy as np
import numpy.typing as npt
import pandas as pd
import json

import grpc
from . import api_pb2_grpc
from . import api_pb2
from google.protobuf import empty_pb2

from typing import Dict, List, Tuple, Union, Optional
Channel = Union[Tuple[str, int], Tuple[str, int, int]]

from .data import NDImage
from .sample import Plate, Well, Site

dtype_to_pb = {
    np.bool8: api_pb2.DataType.BOOL8,
    np.uint8: api_pb2.DataType.UINT8,
    np.uint16: api_pb2.DataType.UINT16,
    np.int16: api_pb2.DataType.INT16,
    np.int32: api_pb2.DataType.INT32,
    np.float32: api_pb2.DataType.FLOAT32,
    np.float64: api_pb2.DataType.FLOAT64,
}

dtype_from_pb = {
    api_pb2.DataType.BOOL8:        np.bool8,
    api_pb2.DataType.UINT8:        np.uint8,
    api_pb2.DataType.UINT16:       np.uint16,
    api_pb2.DataType.INT16:        np.int16,
    api_pb2.DataType.INT32:        np.int32,
    api_pb2.DataType.FLOAT32:      np.float32,
    api_pb2.DataType.FLOAT64:      np.float64,
}

platetype_to_pb = {
    "slide": api_pb2.PlateType.SLIDE,
    "wellplate96": api_pb2.PlateType.WELLPLATE96,
    "wellplate384": api_pb2.PlateType.WELLPLATE384,
}

platetype_from_pb = {
    api_pb2.PlateType.SLIDE: "slide",
    api_pb2.PlateType.WELLPLATE96: "wellplate96",
    api_pb2.PlateType.WELLPLATE384: "wellplate384",
}

class API():
    def __init__(self, server_addr='localhost:50051'):
        self.rpc_channel = grpc.insecure_channel(server_addr, options=[
            ('grpc.max_send_message_length', 20 * 1024 * 1024),
            ('grpc.max_receive_message_length', 20 * 1024 * 1024)
        ])
        self.stub = api_pb2_grpc.NikonTiCtrlStub(self.rpc_channel)

    def list_property(self, name: str) -> List[str]:
        req = api_pb2.ListPropertyRequest(name=name)
        resp = self.stub.ListProperty(req)
        return resp.name

    def get_property(self, name: List[str]) -> Dict[str, str]:
        if isinstance(name, str):
            req = api_pb2.GetPropertyRequest(name=[name])
            resp = self.stub.GetProperty(req)
            return resp.property[0].value
        else:
            req = api_pb2.GetPropertyRequest(name=name)
            resp = self.stub.GetProperty(req)
            return {p.name: p.value for p in resp.property}

    def set_property(self, property_dict: Dict[str, str]) -> None:
        req = api_pb2.SetPropertyRequest()
        for name, value in property_dict.items():
            req.property.append(api_pb2.PropertyValue(name=name, value=value))
        self.stub.SetProperty(req)

    def wait_property(self, name: List[str], timeout_ms: int = 5000) -> None:
        req = api_pb2.WaitPropertyRequest()
        if isinstance(name, str):
            req.name.append(name)
        else:
            req.name.extend(name)
        req.timeout.FromMilliseconds(timeout_ms)
        self.stub.WaitProperty(req)

    def list_channel(self):
        resp = self.stub.ListChannel(empty_pb2.Empty())
        return [(ch.preset_name, ch.exposure_ms, ch.illumination_intensity) for ch in resp.channels]

    def open_experiment(self, name: str, base_dir: Optional[str] = None) -> None:
        if base_dir:
            req = api_pb2.OpenExperimentRequest(name=name, base_dir=base_dir)
        else:
            req = api_pb2.OpenExperimentRequest(name=name)
        self.stub.OpenExperiment(req)

    def _plate_from_pb(self, plate_pb):
        plate = Plate(platetype_from_pb[plate_pb.type], plate_pb.id, self)
        plate._uuid = plate_pb.uuid
        if plate_pb.HasField("pos_origin"):
            plate._pos_origin = (plate_pb.pos_origin.x, plate_pb.pos_origin.y)
        else:
            plate._pos_origin = None
        
        plate._metadata = json.loads(plate_pb.metadata)
        
        for well_pb in plate_pb.well:
            well = Well(plate, well_pb.id)
            well._uuid = well_pb.uuid
            well._rel_pos = (well_pb.rel_pos.x, well_pb.rel_pos.y)
            well._enabled = well_pb.enabled
            well._metadata = json.loads(well_pb.metadata)
            plate._wells[well.id] = well
            
            for site_pb in well_pb.site:
                site = Site(well, site_pb.id, (site_pb.rel_pos.x, site_pb.rel_pos.y))
                site._uuid = site_pb.uuid
                site._enabled = site_pb.enabled
                site._metadata = json.loads(site_pb.metadata)
                well._sites.append(site)
        return plate

    def list_plate(self):
        resp = self.stub.ListPlate(empty_pb2.Empty())
        plate_list = []
        for plate_pb in resp.plate:
            plate_list.append(self._plate_from_pb(plate_pb))
        return plate_list

    def get_plate(self, id):
        return [plate for plate in self.list_plate() if plate._id == id][0]
    
    def add_plate(self, plate_type, plate_id):
        req = api_pb2.AddPlateRequest(
            plate_type=platetype_to_pb[plate_type],
            plate_id=plate_id)
        self.stub.AddPlate(req)
        return
    
    def acquire_multi_channel(self, ndimage_name: str, channels: List[Channel], i_z: int, i_t: int, site_uuid=None, metadata: Dict[str, str] = None):
        req = api_pb2.AcquireMultiChannelRequest()
        req.ndimage_name = ndimage_name
        for ch in channels:
            if len(ch) == 2:
                req.channels.append(api_pb2.Channel(
                    preset_name=ch[0], exposure_ms=ch[1]))
            elif len(ch) == 3:
                req.channels.append(api_pb2.Channel(
                    preset_name=ch[0], exposure_ms=ch[1], illumination_intensity=ch[2]))
            else:
                raise ValueError("invalid channels")
        req.i_z = i_z
        req.i_t = i_t
        if site_uuid:
            req.site_uuid = site_uuid
        if metadata:
            req.metadata = json.dumps(metadata)

        self.stub.AcquireMultiChannel(req)

    def list_ndimage(self):
        resp = self.stub.ListNDImage(empty_pb2.Empty())
        ndimage_list = []
        for im_pb in resp.ndimage:
            ndimage = NDImage(im_pb.name, im_pb.ch_name, im_pb.height, im_pb.width, im_pb.n_ch, im_pb.n_z, im_pb.n_t, api=self)
            ndimage_list.append(ndimage)
        return ndimage_list

    def get_ndimage(self, ndimage_name: str):
        req = api_pb2.GetNDImageRequest(ndimage_name=ndimage_name)
        resp = self.stub.GetNDImage(req)
        im_pb = resp.ndimage
        ndimage = NDImage(im_pb.name, im_pb.ch_name, im_pb.height, im_pb.width, im_pb.n_ch, im_pb.n_z, im_pb.n_t, api=self)
        return ndimage

    def get_image_data(self, ndimage_name: str, channel_name: str, i_z: int, i_t: int):
        req = api_pb2.GetImageDataRequest(
            ndimage_name=ndimage_name, channel_name=channel_name, i_z=i_z, i_t=i_t)
        resp = self.stub.GetImageData(req)

        data_dtype = dtype_from_pb[resp.data.dtype]
        return np.frombuffer(resp.data.buf, dtype=data_dtype).reshape(resp.data.height, resp.data.width)

    def get_segmentation_score(self, im: npt.ArrayLike):
        if len(im.shape) != 2:
            raise ValueError("expecting 2d image")
        req = api_pb2.GetSegmentationScoreRequest()
        req.data.height = im.shape[0]
        req.data.width = im.shape[1]
        req.data.dtype = dtype_to_pb[im.dtype.type]
        req.data.buf = im.tobytes()

        resp = self.stub.GetSegmentationScore(req)

        data_dtype = dtype_from_pb[resp.data.dtype]
        return np.frombuffer(resp.data.buf, dtype=data_dtype).reshape(resp.data.height, resp.data.width)

    def quantify_regions(self, ndimage_name, i_t, segmentation_ch):
        req = api_pb2.QuantifyRegionsRequest(
            ndimage_name=ndimage_name, i_t=i_t, segmentation_ch=segmentation_ch)
        resp = self.stub.QuantifyRegions(req)
        df = []
        for rp in resp.region_prop:
            df.append([rp.label, rp.bbox_x0, rp.bbox_y0, rp.bbox_width, rp.bbox_height, rp.area, rp.centroid_x, rp.centroid_y])
        df = pd.DataFrame(df, columns=["label", "bbox_x0", "bbox_y0", "bbox_width", "bbox_height", "area", "centroid_x", "centroid_y"])

        for ch in resp.raw_intensity:
            df["raw_intensity_%s" % ch.ch_name] = np.array(ch.values)
        return df

    def get_quantification(self, ndimage_name, i_t):
        req = api_pb2.GetQuantificationRequest(
            ndimage_name=ndimage_name, i_t=i_t)
        resp = self.stub.GetQuantification(req)
        df = []
        for rp in resp.region_prop:
            df.append([rp.label, rp.bbox_x0, rp.bbox_y0, rp.bbox_width, rp.bbox_height, rp.area, rp.centroid_x, rp.centroid_y])
        df = pd.DataFrame(df, columns=["label", "bbox_x0", "bbox_y0", "bbox_width", "bbox_height", "area", "centroid_x", "centroid_y"])

        for ch in resp.raw_intensity:
            df["raw_intensity_%s" % ch.ch_name] = np.array(ch.values)
        return df

    def get_xy_stage_position(self) -> Tuple[float, float]:
        x, y = self.get_property("/PriorProScan/XYPosition").split(',')
        return float(x), float(y)

    def set_xy_stage_position(self, x: float, y: float):
        self.set_property(
            {"/PriorProScan/XYPosition": "{:.1f},{:.1f}".format(x, y)})

    def wait_xy_stage(self):
        self.wait_property("/PriorProScan/XYPosition")

    def get_z_stage_position(self) -> float:
        return float(self.get_property("/NikonTi/ZDrivePosition"))

    def set_z_stage_position(self, z: float):
        self.set_property({"/NikonTi/ZDrivePosition": "{:.3f}".format(z)})

    def wait_z_stage(self):
        self.wait_property("/NikonTi/ZDrivePosition")
