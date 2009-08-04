create database django character set utf8;

#enable access from localhost
GRANT all
   ON django.* TO django@localhost
   IDENTIFIED BY 'h2so42';

#enable access from buildslave
GRANT all
   ON django.* TO django@10.0.0.2
   IDENTIFIED BY 'h2so42';



FLUSH PRIVILEGES;
