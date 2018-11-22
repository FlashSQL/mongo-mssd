# mongo-mssd
## Optimize MongoDB with multi-streamed SSD (published [paper](https://link.springer.com/chapter/10.1007/978-981-10-6520-0_1) in [EDB 2017](http://dbsociety.or.kr/edb2017/), accepted [paper](http://jise.iis.sinica.edu.tw/pages/issues/accepted.html) in [JISA Journal](http://jise.iis.sinica.edu.tw/pages/jise/index.html) 2018)

Author: Trong-Dat Nguyen (trauconnguyen@gmail.com)

### Build the source code:

First, make the "build" directory in your MONGO_HOME

```
$ mkdir build
```

Next, to build only the mongod server:
```
$ scons mongod -j40
```

To build all core components (i.e., mognod, mongo, and mongos)

```
$ scons core -j40
```

See [docs/building.md](docs/building.md) for more detail.

## Using Macros
This project support various stream mapping approaches. Each approach has corresponding macro.

To use a desired macro, edit the [SConstruct](SConstruct) file

`MSSD_FILEBASED`: file-based stream mapping

`MSSD_BOUNDBASED`: boundary-based stream mapping

`MSSD_DSM`: Dyanmic Stream Mapping (DSM)
