from typing import Dict, List, Tuple, Union
import time
import math

import numpy as np
import numpy.typing as npt
import grpc
import api_pb2_grpc
import api_pb2

from google.protobuf import empty_pb2

rpc_channel = grpc.insecure_channel('localhost:50051', options=[
    ('grpc.max_send_message_length', 20 * 1024 * 1024),
    ('grpc.max_receive_message_length', 20 * 1024 * 1024)
])
stub = api_pb2_grpc.NikonTiCtrlStub(rpc_channel)

Channel = Union[Tuple[str, int], Tuple[str, int, int]]


def list_property(name: str) -> List[str]:
    req = api_pb2.ListPropertyRequest(name=name)
    resp = stub.ListProperty(req)
    return resp.name


def get_property(name: List[str]) -> Dict[str, str]:
    if isinstance(name, str):
        req = api_pb2.GetPropertyRequest(name=[name])
        resp = stub.GetProperty(req)
        return resp.property[0].value
    else:
        req = api_pb2.GetPropertyRequest(name=name)
        resp = stub.GetProperty(req)
        return {p.name: p.value for p in resp.property}


def set_property(property_dict: Dict[str, str]) -> None:
    req = api_pb2.SetPropertyRequest()
    for name, value in property_dict.items():
        req.property.append(api_pb2.PropertyValue(name=name, value=value))
    stub.SetProperty(req)


def wait_property(name: List[str], timeout_ms: int = 5000) -> None:
    req = api_pb2.WaitPropertyRequest()
    if isinstance(name, str):
        req.name.append(name)
    else:
        req.name.extend(name)
    req.timeout.FromMilliseconds(timeout_ms)
    stub.WaitProperty(req)


def list_channel():
    resp = stub.ListChannel(empty_pb2.Empty())
    return [(ch.preset_name, ch.exposure_ms, ch.illumination_intensity) for ch in resp.channels]


def set_experiment_path(path: str) -> None:
    req = api_pb2.SetExperimentPathRequest(path=path)
    stub.SetExperimentPath(req)


def acquire_multi_channel(ndimage_name: str, channels: List[Channel], i_z: int, i_t: int, metadata: Dict[str, str] = None):
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

    stub.AcquireMultiChannel(req)


def list_ndimage():
    resp = stub.ListNDImage(empty_pb2.Empty())
    ndimage_list = []
    for im in resp.ndimages:
        channel_info = []
        for ch in im.channel_info:
            channel_info.append(
                {"name": ch.name, "width": ch.width, "height": ch.height})
        ndimage_list.append({"name": im.name, "channel_info": channel_info,
                            "n_images": im.n_images, "n_z": im.n_z, "n_t": im.n_t})
    return ndimage_list


dtype_to_api = {
    np.bool8: api_pb2.DataType.BOOL8,
    np.uint8: api_pb2.DataType.UINT8,
    np.uint16: api_pb2.DataType.UINT16,
    np.int16: api_pb2.DataType.INT16,
    np.int32: api_pb2.DataType.INT32,
    np.float32: api_pb2.DataType.FLOAT32,
    np.float64: api_pb2.DataType.FLOAT64,
}

dtype_from_api = {
    api_pb2.DataType.BOOL8:        np.bool8,
    api_pb2.DataType.UINT8:        np.uint8,
    api_pb2.DataType.UINT16:       np.uint16,
    api_pb2.DataType.INT16:        np.int16,
    api_pb2.DataType.INT32:        np.int32,
    api_pb2.DataType.FLOAT32:      np.float32,
    api_pb2.DataType.FLOAT64:      np.float64,
}


def get_image(ndimage_name: str, channel_name: str, i_z: int, i_t: int):
    req = api_pb2.GetImageRequest(
        ndimage_name=ndimage_name, channel_name=channel_name, i_z=i_z, i_t=i_t)
    resp = stub.GetImage(req)

    data_dtype = dtype_from_api[resp.data.dtype]
    return np.frombuffer(resp.data.buf, dtype=data_dtype).reshape(resp.data.height, resp.data.width)


def get_segmentation_score(im: npt.ArrayLike):
    if len(im.shape) != 2:
        raise ValueError("expecting 2d image")
    req = api_pb2.GetSegmentationScoreRequest()
    req.data.height = im.shape[0]
    req.data.width = im.shape[1]
    req.data.dtype = dtype_to_api[im.dtype.type]
    req.data.buf = im.tobytes()

    resp = stub.GetSegmentationScore(req)

    data_dtype = dtype_from_api[resp.data.dtype]
    return np.frombuffer(resp.data.buf, dtype=data_dtype).reshape(resp.data.height, resp.data.width)


def get_xy_stage_position() -> Tuple[float, float]:
    x, y = get_property("/PriorProScan/XYPosition").split(',')
    return float(x), float(y)


def set_xy_stage_position(x: float, y: float):
    set_property({"/PriorProScan/XYPosition": "{:.1f},{:.1f}".format(x, y)})


def wait_xy_stage():
    wait_property("/PriorProScan/XYPosition")


def get_z_stage_position() -> float:
    return float(get_property("/NikonTi/ZDrivePosition"))


def set_z_stage_position(z: float):
    set_property({"/NikonTi/ZDrivePosition": "{:.3f}".format(z)})


def wait_z_stage():
    wait_property("/NikonTi/ZDrivePosition")


def enable_pfs():
    while True:
        pfs_status = get_property("/NikonTi/PFSStatus")
        if pfs_status == "Locked in focus":
            return
        elif pfs_status == "Within range of focus search":
            set_property({"/NikonTi/PFSState": "On"})
            time.sleep(0.1)
            continue
        elif pfs_status == "Focusing":
            time.sleep(0.1)
            continue
        else:
            raise RuntimeError(pfs_status)


def search_focus_range(z_range, z_step_size, z_center, z_max_safety=None):
    try:
        enable_pfs()
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
        set_z_stage_position(z_pos)
        wait_z_stage()
        try:
            enable_pfs()
            return
        except:
            pass

    raise RuntimeError("Focus search failed")
