# Python环境部署文档

## 1、文件上传

​	![1577442309199](C:\Users\v_rcxiao\AppData\Roaming\Typora\typora-user-images\1577442309199.png)

上传以上五个文件， 如果因编码问题出现上传失败，
请参考：https://blog.csdn.net/shmily_lsl/article/det



## 2、编译安装python

### yum更新

个人习惯，安装包之前会更新yum源。

```
sudo yum update
```

### 安装依赖项

安装Python 3.7所需的依赖:

```
sudo yum install zlib-devel bzip2-devel openssl-devel ncurses-devel sqlite-devel readline-devel tk-devel libffi-devel gcc make
```

## 安装Python

通过解压，配置编译，编译安装等步骤完成

### 解压

下载好了之后在文件所在目录解压

```
tar -xvf Python-3.7.0.tgz
```

### 配置编译

进入到解压的python的目录里面，使用`Python3.7.0/configure`文件进行配置

```
cd Python-3.7.0
```

切换root权限：

```
sudo su -
```

配置编译的的路径

```
./configure --prefix=/usr/local/python3
```

**注：**

*这里--prefix是指定编译安装的文件夹*

### 优化选项（可选）：

执行完上一步后会提示执行以下的代码对Python解释器进行优化，***执行该代码后，会编译安装到 /usr/local/bin/ 下，且不用添加软连接或环境变量***

```
./configure --enable-optimizations
```

### 编译和安装

```
make && make install
```

## 添加软连接

添加软链或者添加到环境变量，直接输入python3就可以使用了，下边是添加软连接：

```
sudo ln -s /usr/local/bin/python3 /usr/bin/python
sudo ln -s /usr/local/bin/pip3 /usr/bin/pip
```

## 3、安装其他需求包-按顺序，安装版本依据使用环境自行决定下载

python 3.7环境安装包
```
pip3 install six-1.13.0-py2.py3-none-any.whl				可选，一般sitepackage会带有
pip3 install grpcio-1.25.0-cp37-cp37m-manylinux1_x86_64.whl 必选
pip3 install protobuf-3.11.1-cp37-cp37m-manylinux1_x86_64.whl	必选
pip3 install grpcio_tools-1.25.0-cp37-cp37m-manylinux1_x86_64.whl 可选
```
python 3.6 环境安装包离线安装方式
pip3.6 install --no-index --find-links=./grpcio-1.25.0-cp36-cp36m-manylinux1_x86_64.whl grpcio==1.25.0
pip3.6 install --no-index --find-links=./protobuf-3.12.2-cp36-cp36m-manylinux1_x86_64.whl protobuf==3.12.2
