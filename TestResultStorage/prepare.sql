create database django character set utf8;

GRANT all
   ON django.* TO django@localhost
   IDENTIFIED BY 'h2so42';

GRANT all
   ON django.* TO django@10.0.0.2
   IDENTIFIED BY 'h2so42';



FLUSH PRIVILEGES;
