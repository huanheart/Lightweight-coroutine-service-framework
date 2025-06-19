CREATE DATABASE webserver;
USE webserver;
CREATE TABLE user (
  username CHAR(50) NULL,
  passwd CHAR(50) NULL
) ENGINE=InnoDB;

INSERT INTO user(username, passwd) VALUES('root', '123456');

