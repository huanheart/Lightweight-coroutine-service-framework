# 使用 Ubuntu 22.04 作为基础镜像
FROM ubuntu:22.04

# 设置环境变量，防止安装过程中出现交互式提示
ENV DEBIAN_FRONTEND=noninteractive
# 设置MYSQL root 密码
ENV MYSQL_ROOT_PASSWORD=123456

# 安装基础开发工具、MySQL 服务器、客户端和其他依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    cmake \
    libboost-all-dev \
    mysql-server \
    mysql-client \
    vim \
    libssl-dev \
    libmysqlclient-dev \
    unzip \
    && rm -rf /var/lib/apt/lists/*  # 清理缓存减少镜像体积

# 创建工作目录
WORKDIR /app

# 将宿主机的代码拷贝到容器内的 /app 目录下
COPY . /app

# 复制初始化 SQL 脚本到容器
COPY dockerinit/init.sql /docker-entrypoint-initdb.d/init.sql

# 允许 MySQL 监听所有 IP
RUN echo "[mysqld]\nbind-address=0.0.0.0" >> /etc/mysql/mysql.conf.d/mysqld.cnf

# 启动 MySQL 并设置 root 密码
RUN service mysql start && \
    mysqladmin -u root password "$MYSQL_ROOT_PASSWORD" && \
    mysql -u root -p"$MYSQL_ROOT_PASSWORD" < /docker-entrypoint-initdb.d/init.sql
 

# RUN cat README.md

# 解压 muduo.zip
RUN cd /app/dockerinit 
# unzip一直不成功，先注释掉
RUN unzip /app/dockerinit/muduo.zip -d /app/dockerinit/

# 进入 muduo 目录并执行 build.sh 脚本
RUN cd /app/dockerinit/muduo && ./build.sh

# 进入 build/release-cpp11 执行 make 和 make install
RUN cd /app/dockerinit/build/release-cpp11 && make && make install

# 进入 release-install-cpp11/include 目录并将 muduo 移动到 /usr/include/
RUN mv /app/dockerinit/build/release-install-cpp11/include/muduo /usr/include/

# 进入 lib 目录并将库文件移动到 /usr/local/lib/
RUN mv /app/dockerinit/build/release-install-cpp11/lib/* /usr/local/lib/

# 编译完成后，返回当前工作目录并执行 make
RUN cd /app && make

EXPOSE 9006


# 启动服务并映射到主机的 9006 端口,这里的./CoroutineServer -n 1可进行一个更改参数，或者手动进入容器内部重新启动服务器
CMD cd /app/bin && \
#    ./CoroutineServer -n 1 & \
    service mysql start && \
    tail -f /dev/null

