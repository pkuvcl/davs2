# davs2

遵循 `AVS2-P2/IEEE1857.4` 视频编码标准的解码器. 

对应的编码器 **xavs2** 可在 [Github][2] 或 [Gitee (mirror in China)][3] 上找到.

[![GitHub tag](https://img.shields.io/github/tag/pkuvcl/davs2.svg?style=plastic)]()
[![GitHub issues](https://img.shields.io/github/issues/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/issues)
[![GitHub forks](https://img.shields.io/github/forks/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/network)
[![GitHub stars](https://img.shields.io/github/stars/pkuvcl/davs2.svg)](https://github.com/pkuvcl/davs2/stargazers)

## 编译方法
### Windows

可使用`VS2013`打开解决方案`./build/win32/DAVS2.sln`进行编译, 也可以使用更新的vs版本打开上述解决方案.
打开解决方案后, 将工程`davs2`设置为启动项, 进行编译即可. 

#### 注意
1. 首次编译本项目时, 需要安装一个 `shell 执行器`, 比如 `git-for-windows` 中的 `bash`, 
 需要将该 `bash` 所在的目录添加到系统环境变量 `PATH` 中.
 如上所述, 如果您以默认配置安装了`git-for-windows`, 
 那么将 `C:\Program Files\Git\bin` 添加到环境变量中即可.
2. 需要安装 `vsyasm`, 我们建议的版本号是 `1.2.0`, 因为官方更新的版本存在编译问题.
  下载地址: http://yasm.tortall.net/Download.html .
  一个修改过可以正常编译的 `1.3.0` 版本可以在这里找到: https://github.com/luofalei/yasm/tree/vs2013 .
  其典型的安装步骤如下:
```
(1) 将vsyasm.exe文件拷贝到如下目录: 
    "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\"
(2)	将剩余三个vsyasm文件拷贝到MSBuild模板目录: 
    "C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\V120\BuildCustomizations\"
(3) 重新打开VS2013, asmopt工程应已正常加载, 编译无错误. 
```

### Linux

对于linux系统, 依次执行如下命令即可完成编译:
```
$ cd build/linux
$ ./configure
$ make
```

## 运行和测试

运行命令:
```
./davs2 -i test.avs -o dec.yuv [-r rec.yuv] [-t N]
```

### 参数说明
|       参数       |  等价形式   |   意义           |
| :--------:       | :---------: | :--------------: |
| --input=test.avs | -i test.avs |  设置输入码流文件路径 |
| --output=dec.yuv | -o dec.yuv  |  设置输出解码YUV文件路径 |
| --psnr=rec.yuv   | -r rec.yuv  |  设置参考用YUV文件路径, 用于计算PSNR以确定是否匹配 |
| --threads=N      | -t N        |  设置解码线程数 (默认值: 1) |
| --verbose        | -v          |  设置每帧是否输出 (默认: 开启) |
| --help           | -h          |  显示此输出命令 |

## 主页链接

[北京大学-视频编码算法研究室(PKU-VCL)][1]

`AVS2-P2/IEEE1857.4` Encoder: [xavs2 (Github)][2], [xavs2 (mirror in China)][3]

`AVS2-P2/IEEE1857.4` Decoder: [davs2 (Github)][4], [davs2 (mirror in China)][5]

  [1]: http://vcl.idm.pku.edu.cn/ "PKU-VCL"
  [2]: https://github.com/pkuvcl/xavs2 "xavs2 github repository"
  [3]: https://gitee.com/pkuvcl/xavs2 "xavs2 gitee repository"
  [4]: https://github.com/pkuvcl/davs2 "davs2 decoder@github"
  [5]: https://gitee.com/pkuvcl/davs2 "davs2 decoder@gitee"
