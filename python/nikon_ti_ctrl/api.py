import time
import math

import numpy as np
import numpy.typing as npt

import grpc
from . import api_pb2_grpc
from . import api_pb2
from google.protobuf import empty_pb2

from typing import Dict, List, Tuple, Union
Channel = Union[Tuple[str, int], Tuple[str, int, int]]

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

    def set_experiment_path(self, path: str) -> None:
        req = api_pb2.SetExperimentPathRequest(path=path)
        self.stub.SetExperimentPath(req)

    def acquire_multi_channel(self, ndimage_name: str, channels: List[Channel], i_z: int, i_t: int, metadata: Dict[str, str] = None):
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
        if metadata:
            for key, value in metadata.items():
                req.metadata[key] = value

        self.stub.AcquireMultiChannel(req)

    def list_ndimage(self):
        resp = self.stub.ListNDImage(empty_pb2.Empty())
        ndimage_list = []
        for im in resp.ndimages:
            channel_info = []
            for ch in im.channel_info:
                channel_info.append(
                    {"name": ch.name, "width": ch.width, "height": ch.height})
            ndimage_list.append({"name": im.name, "channel_info": channel_info,
                                "n_images": im.n_images, "n_z": im.n_z, "n_t": im.n_t})
        return ndimage_list

    def get_image(self, ndimage_name: str, channel_name: str, i_z: int, i_t: int):
        req = api_pb2.GetImageRequest(
            ndimage_name=ndimage_name, channel_name=channel_name, i_z=i_z, i_t=i_t)
        resp = self.stub.GetImage(req)

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

    def enable_pfs(self):
        while True:
            pfs_status = self.get_property("/NikonTi/PFSStatus")
            if pfs_status == "Locked in focus":
                return
            elif pfs_status == "Within range of focus search":
                self.set_property({"/NikonTi/PFSState": "On"})
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
            self.set_z_stage_position(z_pos)
            self.wait_z_stage()
            try:
                self.enable_pfs()
                return
            except:
                pass

        raise RuntimeError("Focus search failed")
