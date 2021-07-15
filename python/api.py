import numpy as np
import grpc
import api_pb2_grpc
import api_pb2

channel = grpc.insecure_channel('localhost:50051')
stub = api_pb2_grpc.NikonTiCtrlStub(channel)

def set_property(propertyList):
    propertyValueList = []
    for name, value in propertyList.items():
        propertyValueList.append(api_pb2.PropertyValue(name=name, value=value))
    stub.SetProperty(api_pb2.SetPropertyRequest(property=propertyValueList))

def get_property(name, from_cache=True):
    propertyList = {}
    resp = stub.GetProperty(api_pb2.GetPropertyRequest(name=name, from_cache=from_cache))
    for propertyValue in resp.property:
        propertyList[propertyValue.name] = propertyValue.value
    return propertyList

def list_property(name):
    resp = stub.ListProperty(api_pb2.ListPropertyRequest(name=name))
    return resp.name

def wait_property(name):
    stub.WaitProperty(api_pb2.WaitPropertyRequest(name=name))
    return

def get_image(id):
    resp = stub.GetImage(api_pb2.ImageID(id=id))
    return np.frombuffer(resp.data, dtype=np.uint16).reshape(resp.height, resp.width)

def snap_image(name):
    resp = stub.SnapImage(api_pb2.SnapImageRequest(name=name))
    return resp.id