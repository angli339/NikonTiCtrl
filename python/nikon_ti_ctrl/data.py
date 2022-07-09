import napari

class NDImage():
    def __init__(self, name, data, ch_names):
        self.name = name
        self.data = data
        self.ch_names = ch_names
    
    def view_napari(self):
        napari.view_image(self.data, channel_axis=2,name=self.ch_names)
