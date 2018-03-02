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

### nginx 编译安装

    $ wget http://nginx.org/download/nginx-1.12.2.tar.gz
    $ tar -zxvf nginx-1.12.2.tar.gz
    $ cd nginx-1.12.2
    $ ./configure --prefix=/usr/local/nginx
                  --with-threads
                  --with-file-aio
                  --with-http_ssl_module
                  --with-http_v2_module
                  --without-mail_pop3_module
                  --without-mail_imap_module
                  --without-mail_smtp_module
    $ make
    $ make install

### php 编译安装

    $ wget http://cn2.php.net/distributions/php-7.2.2.tar.gz
    $ tar -zxvf php-7.2.2.tar.gz
    $ cd php-7.2.2
    $ ./configure --prefix=/usr/local/php
                  --enable-fpm
                  --disable-ipv6
                  --enable-mbstring
                  --with-pdo-pgsql=/usr/local/postgresql
                  --with-pgsql=/usr/local/postgresql
    $ make
    $ make install

> 如果 postgresql 是自定义编译安装，`--with-pdo-pgsql` 与 `--with-pgsql` 设置为 postgresql 的安装目录

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

