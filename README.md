# davs2

**davs2** is an open-source decoder of `AVS2-P2/IEEE1857.4` video coding standard.

An encoder, **xavs2**, can be found at [Github][2] or  [Gitee (mirror in China)][3].

[![GitHub tag](https://img.shields.io/github/tag/pkuvcl/davs2.svg?style=plastic)]()
[![GitHub issues](https://img.shields.io/github/issues/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/issues)
[![GitHub forks](https://img.shields.io/github/forks/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/network)
[![GitHub stars](https://img.shields.io/github/stars/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/stargazers)

## Compile it
### Windows
Use VS2013 or latest version of  visual studio open the `./build/vs2013/davs2.sln` solution
 and set the `davs2` as the start project.

#### Notes
1. A `shell executor`, i.e. the bash in git for windows, is needed and should be found in `PATH` variable.
 For example, the path `C:\Program Files\Git\bin` can be added if git-for-windows is installed.
2. `vsyasm` is needed and `1.2.0` is suggested for windows platform.
 It can be downloaded through: http://yasm.tortall.net/Download.html .
 A later version `1.3.0` (unofficial revision, please read the instructions of `yasm` to build it for your work), can be found in https://github.com/luofalei/yasm/tree/vs2013 .
   The installation of `vsyasm` is as follows (if you were using `VS2013`):
```
(1) Copy `syasm.exe` to the following directory, 
    "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\"
(2)	Copy the other 3 files in `vsyasm` to the `MSBuild template` directorty, as follows, 
    "C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\V120\BuildCustomizations\"
(3) Re-open the solution. 
```

### Linux
```
$ cd build/linux
$ ./configure
$ make
```

## Try it

```
./davs2 -i test.avs -o test_dec.yuv [-r test_rec.yuv] [-t N]
```

### Parameter Instructions
|  Parameter       |   Alias     |   Result  |
| :--------:       | :---------: | :--------------: |
| --input=test.avs | -i test.avs |  Setting the input bitstream file |
| --output=dec.yuv | -o dec.yuv  |  Setting the output YUV file |
| --psnr=rec.yuv   | -r rec.yuv  |  Setting the reference reconstruction YUV file |
| --threads=N      | -t N        |  Setting the threads for decoding (default: 1) |
| --verbose        | -v          |  Enable decoding status every frame (Default: Enabled) |
| --help           | -h          |  Showing this instruction |

## Homepages

[PKU-VCL][1]

`AVS2-P2/IEEE1857.4` Encoder: [xavs2 (Github)][2], [xavs2 (mirror in China)][3]

`AVS2-P2/IEEE1857.4` Decoder: [davs2 (Github)][4], [davs2 (mirror in China)][5]

  [1]: http://vcl.idm.pku.edu.cn/ "PKU-VCL"
  [2]: https://github.com/pkuvcl/xavs2 "xavs2 github repository"
  [3]: https://gitee.com/pkuvcl/xavs2 "xavs2 gitee repository"
  [4]: https://github.com/pkuvcl/davs2 "davs2 decoder@github"
  [5]: https://gitee.com/pkuvcl/davs2 "davs2 decoder@gitee"
