# DMR Voice Relay Server

一个简单的DMR（Digital Mobile Radio）语音转发服务器，用于中继DMR语音和数据通信。

## 功能特点

- 支持DMR语音和数据包的转发
- 支持多客户端连接
- 自动客户端管理和超时清理
- 跨平台支持（Windows和Linux）
- 可配置的服务器参数
- 支持数据库用户认证
- 支持DMR ID数据库管理

## 技术规格

- 使用UDP协议进行通信
- 支持标准DMR帧格式
- 支持DMR时隙1和时隙2
- 支持DMR ID识别和管理
- 支持MariaDB/MySQL数据库连接

## 编译说明

### 依赖项

- C编译器 (GCC/MinGW/MSVC)
- Make工具
- MariaDB/MySQL客户端库 (用于数据库功能)

### Windows

1. 确保已安装MinGW或Visual Studio
2. 安装MariaDB/MySQL客户端库
3. 打开命令提示符并导航到项目目录
4. 运行 `mingw32-make` 或 `nmake -f Makefile.win`

### Linux/Unix

1. 确保已安装GCC和Make
2. 安装MariaDB/MySQL开发库: `apt-get install libmariadb-dev` 或 `yum install mariadb-devel`
3. 打开终端并导航到项目目录
4. 运行 `make`

## 使用方法

```
dmr_server [options]

选项:
  -p PORT     服务器端口 (默认: 62031)
  -b ADDR     绑定地址 (默认: 任意)
  -t TIMEOUT  客户端超时时间(秒) (默认: 300)
  -v          详细输出模式
  -h          显示帮助信息
  
  # 数据库选项
  --db-enable           启用数据库功能
  --db-host HOST        数据库服务器地址 (默认: localhost)
  --db-user USER        数据库用户名
  --db-pass PASSWORD    数据库密码
  --db-name NAME        数据库名称 (默认: dmr_server)
  --db-auth             启用数据库用户认证
```

## 示例

启动服务器并监听默认端口:
```
dmr_server
```

指定端口和详细输出模式:
```
dmr_server -p 12345 -v
```

绑定到特定IP地址:
```
dmr_server -b 192.168.1.10
```

启用数据库功能和用户认证:
```
dmr_server --db-enable --db-host localhost --db-user dmruser --db-pass password123 --db-name dmr_database --db-auth
```

## 数据库配置

服务器支持使用MariaDB/MySQL数据库进行用户认证和DMR ID管理。

### 数据库表结构

用户认证表 (`users`):
```sql
CREATE TABLE users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  dmr_id INT NOT NULL UNIQUE,
  callsign VARCHAR(10) NOT NULL,
  password VARCHAR(64) NOT NULL,  -- 存储密码哈希
  active BOOLEAN DEFAULT TRUE,
  last_login DATETIME,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

DMR ID表 (`dmr_ids`):
```sql
CREATE TABLE dmr_ids (
  id INT AUTO_INCREMENT PRIMARY KEY,
  dmr_id INT NOT NULL UNIQUE,
  callsign VARCHAR(10) NOT NULL,
  name VARCHAR(100),
  country VARCHAR(50),
  city VARCHAR(50),
  state VARCHAR(50),
  access_allowed BOOLEAN DEFAULT TRUE,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);
```

### 数据库初始化

在 `database` 目录中提供了初始化脚本:
```
database/init.sql
```

可以使用以下命令导入:
```
mysql -u username -p database_name < database/init.sql
```

## 协议说明

DMR服务器使用以下帧格式:

```
+--------+--------+--------+--------+--------+--------+--------+--------+----------------+
| 类型   | 时隙   |        源DMR ID         |        目标DMR ID        |     载荷数据    |
+--------+--------+--------+--------+--------+--------+--------+--------+----------------+
|  1字节 |  1字节 |        3字节           |        3字节            |   最多27字节     |
+--------+--------+--------+--------+--------+--------+--------+--------+----------------+
```

## 许可证

本项目采用MIT许可证。详情请参阅LICENSE文件。

## 贡献

欢迎提交问题报告和改进建议。如果您想贡献代码，请先创建一个问题讨论您的想法。