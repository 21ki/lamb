### lamb 配置手册

lamb 是命令行控制台管理工具，所有和业务无关的核心、高级功能都使用 lamb 工具来操作。

#### 登录 lamb 系统

默认情况下 lamb 会作为 Linux 的终端 shell。使用任何一个 ssh 客户端即可使用

    $ ssh lamb@192.168.1.200

#### lamb 命令列表

    help                        显示所有 lamb 命令帮助信息
    show mt                     显示核心 MT 上行消息队列统计信息
    show mo                     显示核心 MO 下行消息队列统计信息
    show core                   显示核心模块的运行状态信息
    show client                 显示当前 cmpp 客户端连接信息
    show server                 显示所有 server 业务处理进程信息
    show account                显示所有 account 账户信息
    show channel                显示运营商 channel 网关状态信息
    show gateway                显示运营商网关账户信息
    show delivery               显示下行推送路由表信息
    show routing <id>           显示账号的上行路由表信息
    show log <type>             查看模块日志输出信息
    kill client <id>            强制断开一个 cmpp 客户端连接
    kill server <id>            强制一个 server 业务进程退出
    kill channel <id>           强制一个 channel 网关退出下线
    start server <id>           启动一个 server 业务处理进程
    start channel <id>          启动一个 channel 网关通道连接
    change password <password>  修改 lamb 管理平台的用户密码
    show version                显示 lamb 系统的版本信息
    exit                        退出 lamb 系统登录

#### show mt

    Id Account Total  Description
    ---------------------------------------------------
    1  null    983    no description
    2  null    0      no description
    13 null    0      no description
    14 null    0      no description
    16 null    0      no description

`Id` 表示对应 account 账户编号，`Total` 表示当前等待处理的上行消息总数

#### show mo

    Id Account Total  Description
    ---------------------------------------------------
    1  null    983    no description
    2  null    0      no description
    13 null    0      no description
    14 null    0      no description
    16 null    0      no description

`Id` 表示对应 account 账户编号，`Total` 表示当前等待推送给客户端的下行消息总数

#### show core

    Pid Module    Status Description
    -----------------------------------------------------------
    29509 ismg        ok   client cmpp access gateway
    20303 mt          ok   core uplink message queue service
    20308 mo          ok   core downlink message queue service
    20315 scheduler   ok   core routing scheduler service
    20321 delivery    ok   core delivery scheduler service
    20330 testd       ok   carrier gateway test service
    20337 daemon      ok   daemon management service

`Pid` 表示模块进程 PID，`Module` 表示模块名称，`Status` 表示模块当前运行状态

#### show client

    Id User   Company   Host            Status Speed Error
    ----------------------------------------------------------
    1  901234 1         192.168.1.254     ok   10    0
    2  901235 1         192.168.1.247     ok   10    0

`Id` 表示客户端账户编号，`User` 表示客户端账号名称， `Company` 表示客户端所属公司编号，`Host` 客户端连接IP地址，`Status` 客户端连接状态，`Speed` 客户端消息发送速率，`Error` 客户端消息错误统计

#### show server

    Id Pid   Status store bill  blk   usb   limt  rejt  tmp   key
    -----------------------------------------------------------------
     1 30235   ok   0     0     0     0     0     0     0     0
     2 0       no   0     0     0     0     0     0     0     0
    13 0       no   0     0     0     0     0     0     0     0
    14 0       no   0     0     0     0     0     0     0     0
    16 0       no   0     0     0     0     0     0     0     0

`Id` 业务进程的账户编号， `Pid` 业务处理进程的PID， `Status` 业务进程状态，`store` 等待存储的消息总数， `bill` 等待计费的消息总数, `blk` 黑名单拦截总数，  `usb` 退订拦截总数，  `limt` 频次限制拦截总数， `rejt` 被路由调度器拒绝总数， `tmp` 模板签名验证失败统计，  `key` 关键字过滤拦截总数

#### show channel

    Id Name        Type Host            Status Speed Error
    ----------------------------------------------------------
     1 test        cmpp 192.168.1.156     ok   0     0
    16 lz2018xyk   cmpp 110.160.97.18     no   0     0
    17 demo        cmpp 212.25.19.100     no   0     0

`Id` 网关通道编号， `Name` 网关名称，  `Type` 网关类型， `Host` 网关IP地址，  `Status` 网关通道当前状态， `Speed` 网关通道当前消息处理速率， `Error` 网关通道错误统计

#### show delivery

    Id Rexp                     Account
    ---------------------------------------------------
    1 ^.*$                      1
    2 ^10690008                 1

`Id` 路由编号， `Rexp` 路由规则， `Account` 目标推送账号编号

#### kill client

示例：

    lamb> kill client 1

表示强制断开账户编号为 1 的客户端连接

#### kill server

示例：

    lamb> kill server 1

表示强制账户编号为 1 的业务进程退出停止服务

#### kill channel

示例：

    lamb> kill channel 1

表示强制网关编号为 1 的通道断开连接并退出

#### start server

示例：

    lamb> start server 2

表示启动一个账户编号为 2 的业务处理进程

#### start channel

示例：

    lamb> start channel 1

表示启动一个网关编号为 2 的通道连接服务

#### change password

示例：

    lamb> change password 123456

表示将 lamb 用户登录密码修改为 123456


















