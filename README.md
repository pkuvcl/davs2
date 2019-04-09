# davs2
**davs2** is an open-source decoder of `AVS2-P2/IEEE1857.4` video coding standard.

An encoder, **xavs2**, can be found at [Github][2] or  [Gitee (mirror in China)][3].

[![GitHub tag](https://img.shields.io/github/tag/pkuvcl/davs2.svg?style=plastic)]()
[![GitHub issues](https://img.shields.io/github/issues/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/issues)
[![GitHub forks](https://img.shields.io/github/forks/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/network)
[![GitHub stars](https://img.shields.io/github/stars/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/stargazers)

Linux(Ubuntu-16.04):[![Travis Build Status](https://travis-ci.org/pkuvcl/davs2.svg?branch=master)](https://travis-ci.org/pkuvcl/davs2)
Windows(VS2013):[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/pq0b5mnc6mig6ryp?svg=true)](https://ci.appveyor.com/project/luofalei/davs2/build/artifacts)

Stargazers over time
[![Stargazers over time](https://starcharts.herokuapp.com/pkuvcl/davs2.svg)](https://starcharts.herokuapp.com/pkuvcl/davs2)

## Compile it
### Windows
Use VS2013 or latest version of  visual studio open the `./build/vs2013/davs2.sln` solution
 and set the `davs2` as the start project.

#### Notes
1. A `shell executor`, i.e. the bash in git for windows, is needed and should be found in `PATH` variable.
 For example, the path `C:\Program Files\Git\bin` can be added if git-for-windows is installed.
2. `nasm.exe` with version `2.13` (or later version) is needed and should be put into the `PATH` directory.
 For windows platform, you can downloaded the packege and unpack the zip file to get `nasm.exe`:
https://www.nasm.us/pub/nasm/releasebuilds/2.14.02/win64/nasm-2.14.02-win64.zip

### Linux
```
$ cd build/linux
$ ./configure
$ make
```

## Try it

Decode AVS2 stream `test.avs` with `1` thread and output to a *YUV file* named `dec.yuv`.
```
./davs2 -i test.avs -t 1 -o dec.yuv
```

Decode AVS2 stream `test.avs` and display the decoding result via *ffplay*.
```
./davs2 -i test.avs -t 1 -o stdout | ffplay -i -
```

### Parameter Instructions
|  Parameter       |   Alias     |   Result  |
| :--------:       | :---------: | :--------------: |
| --input=test.avs | -i test.avs |  Setting the input bitstream file |
| --output=dec.yuv | -o dec.yuv  |  Setting the output YUV file |
| --psnr=rec.yuv   | -r rec.yuv  |  Setting the reference reconstruction YUV file |
| --threads=N      | -t N        |  Setting the threads for decoding (default: 1) |
| --md5=M          | -m M        |  Reference MD5, used to check whether the output YUV is right |
| --verbose        | -v          |  Enable decoding status every frame (Default: Enabled) |
| --help           | -h          |  Showing this instruction |

## Issue and Pull Request

[Issues should be reported here][6]。

If you have some bugs fixed or features implemented, and would like to share with the public, please [make a Pull Request][7].

## Homepages

[PKU-VCL][1]

`AVS2-P2/IEEE1857.4` Encoder: [xavs2 (Github)][2], [xavs2 (mirror in China)][3]

`AVS2-P2/IEEE1857.4` Decoder: [davs2 (Github)][4], [davs2 (mirror in China)][5]

  [1]: http://vcl.idm.pku.edu.cn/ "PKU-VCL"
  [2]: https://github.com/pkuvcl/xavs2 "xavs2 github repository"
  [3]: https://gitee.com/pkuvcl/xavs2 "xavs2 gitee repository"
  [4]: https://github.com/pkuvcl/davs2 "davs2 decoder@github"
  [5]: https://gitee.com/pkuvcl/davs2 "davs2 decoder@gitee"
  [6]: https://github.com/pkuvcl/davs2/issues "report issues"
  [7]: https://github.com/pkuvcl/davs2/pulls "pull request"
