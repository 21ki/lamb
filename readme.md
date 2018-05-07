This a Open Source SMS Gateway Platform, Support the CMPP 2.0 protocol

### 基础环境

- CentOS 7.3
- nginx 1.12
- php 7.2
- yaf 3.0
- redis 4.0
- cmpp 1.0
- hiredis 0.13
- libiconv 1.15
- postgresql 10.2
- protobuf 3.5.1
- protobuf-c 1.3.0

### 依赖软件库

    $ yum update -y
    $ yum install -y epel-release
    $ yum install -y gcc gcc-c++ make cmake automake autoconf libtool openssl-devel curl-devel
    $ yum install -y git wget re2c re2c-devl flex flex-devel bison bison-devl libconfig-devel
    $ yum install -y pcre-devel readline-devel libxml2-devel hiredis-devel libpqxx-devel

### 其他组件安装方法

* [nginx 安装方法](https://github.com/typefo/blog/blob/master/nginx/nginx-compile-install.md)
* [php 安装方法](https://github.com/typefo/blog/blob/master/php/php7-compile-install.md)
* [postgresql 安装方法](https://github.com/typefo/blog/blob/master/postgresql/postgresql10-compile-install.md)

### 内核参数配置 /etc/sysctl.conf

    net.ipv6.conf.all.disable_ipv6 = 1
    net.ipv6.conf.default.disable_ipv6 = 1
    net.ipv4.ip_forward = 1
    net.ipv4.tcp_syncookies = 1
    net.ipv4.tcp_tw_reuse = 1
    net.ipv4.tcp_tw_recycle = 1
    net.ipv4.tcp_fin_timeout = 15
    net.ipv4.tcp_synack_retries = 2
    fs.file-max = 2048000
    fs.nr_open = 2048000
    fs.file-max = 1024000
    fs.aio-max-nr = 1048576

### 内核参数配置 /etc/security/limits.conf

    * soft    nofile  1024000
    * hard    nofile  1024000
    * soft    nproc   unlimited
    * hard    nproc   unlimited
    * soft    core    unlimited
    * hard    core    unlimited
    * soft    memlock unlimited
    * hard    memlock unlimited

### protobuf 编译安装

    $ wget https://github.com/google/protobuf/releases/download/v3.5.1/protobuf-cpp-3.5.1.tar.gz
    $ tar -zxvf protobuf-cpp-3.5.1.tar.gz
    $ cd protobuf-3.5.1
    $ ./configure
    $ make
    $ make install
    $ export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

### protobuf-c 编译安装

    $ wget https://github.com/protobuf-c/protobuf-c/releases/download/v1.3.0/protobuf-c-1.3.0.tar.gz
    $ cd protobuf-c-1.3.0
    $ ./configure
    $ make
    $ make install

### cmpp 协议库安装

    $ git clone https://github.com/typefo/cmpp.git
    $ cd cmpp
    $ make
    $ make install

### lamb 核心模块安装

    $ make install

### lamb 命令行工具文档

[lamb命令手册](doc/lamb.md)
