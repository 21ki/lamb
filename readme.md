This a SMS Gateway Platform, Support the CMPP protocol

### 内核参数配置 (/etc/sysctl.conf)

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
    fs.mqueue.msg_default = 512
    fs.mqueue.msg_max = 65536
    fs.mqueue.msgsize_default = 512
    fs.mqueue.msgsize_max = 512
    fs.mqueue.queues_max = 1024

### 内核参数配置 (/etc/security/limits.conf)

    * soft    nofile  1024000
    * hard    nofile  1024000
    * soft    nproc   unlimited
    * hard    nproc   unlimited
    * soft    core    unlimited
    * hard    core    unlimited
    * soft    memlock unlimited
    * hard    memlock unlimited
