# nestdaq new-device command

## Usage
```
Usage: new-device [options]

Options:
  --prefix           Perfix that points to the directory path to which files are output
                     (default = <current working directory>/my_nestdaq_impl)
  --project          Project name used in cmake.
                     (default - my_nestdaq_impl)
  --device           NestDAQ Device name. 
                     If there are multiple devices, use "A;B;C" like in a cmake list.
  -h,--help          Show this help message
```