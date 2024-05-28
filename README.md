# ADScanPB

AreaDetector driver that allows for re-playing scans collected with physical detectors. Allows for much simpler iteration of live and real time analysis software.

### Installation

ADScanPB can be built both with or without support for reading data directly from a [`tiled`](https://github.com/bluesky/tiled) server


### Adding a new data source

ADScanPB was written in such a way that additional data sources should be relatively simple to add. Essentially, since once the data is read into memory the original source of it is no longer relevant, all that is needed to support additional data sources is to write a single new function, and add it to the list of options selectable from the `DataSource` PV.

Your function simply needs to guarantee a few things:

* The dimensions of each image along with the data type and color mode are loaded into the corresponding parameters.
* The actual image data is `memcpy`'d into the `imageData` buffer, with each image being stored in order, row first, from the top to the bottom of the image.
* The number of frames is known and read into the approprate PV.

If your data source requires more than one or two string variables as identifiers, you may also need to add additional PVs that can be used. 
