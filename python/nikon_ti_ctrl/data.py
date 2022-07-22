import napari
import numpy as np
from typing import List

class NDImage():
    def __init__(self, name: str, ch_names: List[str], height, width, n_ch, n_z, n_t, api=None):
        self._name = name
        self._ch_names = ch_names
        self._height = height
        self._width = width
        self._n_ch = n_ch
        self._n_z = n_z
        self._n_t = n_t
        self._data = None
        self._api = api

    @property
    def data(self):
        if not self._data is None:
            return self._data
        
        ims = []
        for i_t in range(self._n_t):
            for i_z in range(self._n_z):
                for ch in self._ch_names:
                    im = self._api.get_image_data(self._name, ch, i_z, i_t)
                    ims.append(im)
        ims = np.array(ims)
        ims = ims.reshape([self._n_t, self._n_z, self._n_ch, self._height, self._width])
        self._data = ims
        return self._data

    def view_napari(self):
        napari.view_image(self.data, channel_axis=2, name=self._ch_names)
