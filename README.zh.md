# davs2

遵循 `AVS2-P2/IEEE1857.4` 视频编码标准的解码器. 

对应的编码器 **xavs2** 可在 [Github][2] 或 [Gitee (mirror in China)][3] 上找到.

[![GitHub tag](https://img.shields.io/github/tag/pkuvcl/davs2.svg?style=plastic)]()
[![GitHub issues](https://img.shields.io/github/issues/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/issues)
[![GitHub forks](https://img.shields.io/github/forks/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/network)
[![GitHub stars](https://img.shields.io/github/stars/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/stargazers)

Linux(Ubuntu-16.04):[![Travis Build Status](https://travis-ci.org/pkuvcl/davs2.svg?branch=master)](https://travis-ci.org/pkuvcl/davs2)
Windows(VS2013):[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/pq0b5mnc6mig6ryp?svg=true)](https://ci.appveyor.com/project/luofalei/davs2/build/artifacts)

[![Stargazers over time](https://starcharts.herokuapp.com/pkuvcl/davs2.svg)](https://starcharts.herokuapp.com/pkuvcl/davs2)

## 编译方法
### Windows

可使用`VS2013`打开解决方案`./build/win32/DAVS2.sln`进行编译, 也可以使用更新的vs版本打开上述解决方案.
打开解决方案后, 将工程`davs2`设置为启动项, 进行编译即可. 

#### 注意
1. 首次编译本项目时, 需要安装一个 `shell 执行器`, 比如 `git-for-windows` 中的 `bash`, 
 需要将该 `bash` 所在的目录添加到系统环境变量 `PATH` 中.
 如上所述, 如果您以默认配置安装了`git-for-windows`, 
 那么将 `C:\Program Files\Git\bin` 添加到环境变量中即可.
2. 需将 `nasm.exe`放入到系统 `PATH` 目录, `nasm`版本号需为`2.13`或更新.
  对于`windows`平台,可下载如下压缩包中，解压得到`nasm.exe`.
https://www.nasm.us/pub/nasm/releasebuilds/2.14.02/win64/nasm-2.14.02-win64.zip

### Linux

对于linux系统, 依次执行如下命令即可完成编译:
```
$ cd build/linux
$ ./configure
$ make
```

## 运行和测试

使用`1`个线程解码AVS2码流文件`test.avs`并将结果输出成YUV文件`dec.yuv`:
```
./davs2 -i test.avs -t 1 -o dec.yuv
```

解码AVS2码流文件`test.avs`并用ffplay播放显示:
```
./davs2 -i test.avs -t 1 -o stdout | ffplay -i -
```

### 参数说明
|       参数       |  等价形式   |   意义           |
| :--------:       | :---------: | :--------------: |
| --input=test.avs | -i test.avs |  设置输入码流文件路径 |
| --output=dec.yuv | -o dec.yuv  |  设置输出解码YUV文件路径 |
| --psnr=rec.yuv   | -r rec.yuv  |  设置参考用YUV文件路径, 用于计算PSNR以确定是否匹配 |
| --threads=N      | -t N        |  设置解码线程数 (默认值: 1) |
| --md5=M          | -m M        |  设置参考MD5值, 用于验证输出的重构YUV是否匹配 |
| --verbose        | -v          |  设置每帧是否输出 (默认: 开启) |
| --help           | -h          |  显示此输出命令 |

## Issue & Pull Request

欢迎提交 issue，请写清楚遇到问题的环境与运行参数，包括操作系统环境、编译器环境等。
如果可能提供原始输入`YUV/码流文件`，请尽量提供以方便更快地重现结果。

[反馈问题的 issue 请按照模板格式填写][6]。

如果有开发能力，建议在本地调试出错的代码，并[提供相应修正的 Pull Request][7]。

## 主页链接

[北京大学-视频编码算法研究室(PKU-VCL)][1]

`AVS2-P2/IEEE1857.4` Encoder: [xavs2 (Github)][2], [xavs2 (mirror in China)][3]

`AVS2-P2/IEEE1857.4` Decoder: [davs2 (Github)][4], [davs2 (mirror in China)][5]

  [1]: http://vcl.idm.pku.edu.cn/ "PKU-VCL"
  [2]: https://github.com/pkuvcl/xavs2 "xavs2 github repository"
  [3]: https://gitee.com/pkuvcl/xavs2 "xavs2 gitee repository"
  [4]: https://github.com/pkuvcl/davs2 "davs2 decoder@github"
  [5]: https://gitee.com/pkuvcl/davs2 "davs2 decoder@gitee"
  [6]: https://github.com/pkuvcl/davs2/issues "report issues"
  [7]: https://github.com/pkuvcl/davs2/pulls "pull request"
