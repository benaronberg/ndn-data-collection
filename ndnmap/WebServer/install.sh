#!/bin/bash

# This is an Install script that works on Ubuntu 12.04. That is the only place it has been tested.

SCRIPT_NAME=$0
set -e # die on any error
export DEBIAN_FRONTEND=noninteractive

#./install_mysql 
#./init_app_db -d ndnmap -u ndnmap -p mysql
# ./install.sh -D ndnmap -n ndnmap -U ndnmap -w example.com -s http://www.arl.wustl.edu/~jdd/NDN/static -P mysql -g https://WU-ARL@github.com/WU-ARL/ndnmap.git -u ndnmap -p /home/ndnmap/ndnmap -f /home/ndnmap/ndnmap/WebServer/deploy/initial_data.json 

usage() {
    cat <<EOF
Usage: $SCRIPT_NAME OPTIONS

OPTIONS
    Application:
     -r                 restart only. Nothing is installed
     -n NAME            application NAME - letters, numbers and underscore (REQUIRED)
     -g GITREPO         URL of the git repo to clone (REQUIRED)
     -b BRANCH          git branch name 

    Database:
     -H DB_HOST         database host
     -D DB_NAME         database name: default = \${NAME} (REQUIRED)
     -U DB_USER         database user: default = \${NAME} (REQUIRED)
     -P DB_PASS         database password (REQUIRED)

    Web:
     -w SITE_NAME       SiteName for Apache: default =  \${NAME}.com
     -s STATIC_URL      URL for static assets: default = /static/
      -l URL            URL sql dump tarball to load into the database

    Local:
     -u LOCAL_USER      name of system user to add: defaults to \${NAME}
     -p PROJECT_ROOT    project root: default = /home/\${LOCAL_USER}/\${NAME}
     -f FIXTURE         initial data: default = PROJECT_ROOT/deploy/initial_data.json
EOF
}

die() {
    message=$1
    error_code=$2

    echo "$SCRIPT_NAME: $message" 1>&2
    usage
    exit $error_code
}
check_arg() {
    arg=$1
    if [ -z "${!arg}" ]; then 
        die "$arg is required" 1
    fi
}

getargs() {
    RESTART="FALSE"
    while getopts "rhn:l:g:b:H:D:U:P:w:s:u:p:f:" opt; do
        case "$opt" in
            h) usage; exit 0 ;;
            r) RESTART="TRUE";;
            n) NAME="$OPTARG" ;;
            g) GITREPO="$OPTARG" ;;
            b) BRANCH="$OPTARG" ;;
            H) DB_HOST="$OPTARG" ;;
            D) DB_NAME="$OPTARG" ;;
            U) DB_USER="$OPTARG" ;;
            P) DB_PASS="$OPTARG" ;;
            w) SITE_NAME="$OPTARG" ;;
            l) URL="$OPTARG";;
            s) STATIC_URL="$OPTARG" ;;
            u) LOCAL_USER="$OPTARG" ;;
            p) PROJECT_ROOT="$OPTARG" ;;
            f) FIXTURE="$OPTARG" ;;
            [?]) die "unknown option $opt" 10 ;;
        esac
    done
    # check required args
    check_arg "NAME"
    check_arg "GITREPO"
    check_arg "DB_NAME"
    check_arg "DB_USER"
    check_arg "DB_PASS"
}
setdefaults() {
    if [ -z "$DB_NAME" ]; then
        DB_NAME="$NAME"
    fi
    if [ -z "$DB_HOST" ]; then
        DB_HOST="localhost"
    fi
    if [ -z "$DB_USER" ]; then
        DB_USER="$NAME"
    fi
    if [ -z "$DB_PASS" ]; then
        DB_PASS=`head -c 100 /dev/urandom | md5sum | awk '{print $1}'`
    fi
    if [ -z "$SITE_NAME" ]; then
        SITE_NAME="${NAME}.com"
    fi
    if [ -z "$STATIC_URL" ]; then
        STATIC_URL="/static/"
    fi
    if [ -z "$ADMIN_EMAIL" ]; then
        ADMIN_EMAIL="alerts@${SITE_NAME}"
    fi
    if [ -z "$LOCAL_USER" ]; then
        LOCAL_USER="$NAME"
    fi
    if [ -z "$PROJECT_ROOT" ]; then
        PROJECT_ROOT="/home/$LOCAL_USER/$NAME" 
    fi
    if [ -z "$BRANCH" ]; then
        BRANCH="master" 
    fi
    if [ -z "$FIXTURE" ]; then
        FIXTURE="$PROJECT_ROOT/deploy/initial_data.json" 
    fi
}


#CREATE USER '$DB_USER'@'localhost' IDENTIFIED by '$DB_PASS';
create_mysql_database() {
    cat <<EOF | mysql --user=root -p$DB_PASS
CREATE USER "$DB_USER" IDENTIFIED by "$DB_PASS";
CREATE DATABASE IF NOT EXISTS $DB_NAME;
GRANT ALL PRIVILEGES  on $DB_NAME.* to '$DB_USER'@'localhost' identified by '$DB_PASS';
EOF
    # create options file
    touch ~/.my.cnf
    chmod 600 ~/.my.cnf
}

open_external_port() {
    cat <<EOF | sudo tee /etc/mysql/conf.d/listen_externally.cnf
[mysqld]
    bind-address = 0.0.0.0
EOF
    sudo /etc/init.d/mysql restart
}

load_sql() {
    if [ -z "$URL" ]; then 
        echo "No url specified. Creating an empty database."
        return 0 
    fi 
    wget $URL -O dump.sql.tgz
    tar zxvf dump.sql.tgz
    mysql --user=$DB_USER --password=$DB_PASS $DB_NAME < dump.sql
}

print_mysql_config() {
    #PUBLIC_DNS=`curl http://169.254.169.254/latest/meta-data/public-hostname 2>/dev/null`
    cat <<EOF
Database: $DB_NAME
Username: $DB_USER
Password: $DB_PASS

EOF
}

update_system() {
    sudo apt-get update && apt-get upgrade -y
}

install_baseline() {
    sudo apt-get install -y build-essential git-core curl
}

install_python() {
    sudo apt-get install -y python python-dev python-pip python-setuptools python-mysqldb python-virtualenv
}

install_webserver() {
    # install apache
    sudo apt-get install -y apache2 libapache2-mod-wsgi 
    # configure apache
    APACHE_LOG_DIR=/var/log/apache2 #fix for Ubuntu 10.04 (Lucid) 
    cat <<EOF | sudo tee /etc/apache2/sites-available/$NAME
<VirtualHost *:80>
    ServerName $SITE_NAME
    ServerAdmin $ADMIN_EMAIL
    LogLevel warn
    ErrorLog ${APACHE_LOG_DIR}/${SITE_NAME}_error.log
    CustomLog ${APACHE_LOG_DIR}/${SITE_NAME}_access.log combined

    #WSGIDaemonProcess $NAME user=www-data group=www-data maximum-requests=10000 python-path=/home/$LOCAL_USER/env/lib/python2.7/site-packages
    WSGIDaemonProcess $NAME user=www-data group=www-data maximum-requests=10000 
    WSGIProcessGroup $NAME

    WSGIScriptAlias / $PROJECT_ROOT/WebServer/deploy/app.wsgi

    <Directory $PROJECT_ROOT/WebServer/deploy>
        Order deny,allow
        Allow from all
    </Directory>

</VirtualHost>
EOF

}

activate_webserver() {
    sudo a2dissite default
    sudo a2ensite $NAME
    sudo /etc/init.d/apache2 reload
}

bootstrap_project() {
    #sudo pip install virtualenv    
    sudo adduser --system --disabled-password --disabled-login $LOCAL_USER
    #sudo -u $LOCAL_USER virtualenv /home/$LOCAL_USER/env
    sudo -u $LOCAL_USER mkdir -p $PROJECT_ROOT    
}

install_project1() {
    # clone from git hub
    # assumes read-only repo
    sudo -u $LOCAL_USER mkdir -p /home/$LOCAL_USER/.ssh    
    sudo -u $LOCAL_USER ssh-keyscan -H github.com | sudo -u $LOCAL_USER tee /home/$LOCAL_USER/.ssh/known_hosts
    sudo -u $LOCAL_USER git clone -b $BRANCH $GITREPO $PROJECT_ROOT
}

install_project2() {
    sudo easy_install -U distribute
    #sudo -u $LOCAL_USER /home/$LOCAL_USER/env/bin/pip install -r $PROJECT_ROOT/WebServer/deploy/requirements.txt
    #sudo -u $LOCAL_USER pip install -r $PROJECT_ROOT/WebServer/deploy/requirements.txt
    sudo pip install -r $PROJECT_ROOT/WebServer/deploy/requirements.txt
    # Extra stuff that might have been missed in 12.04
    sudo apt-get install libmysqlclient-dev
    #sudo -u ndnmap /home/ndnmap/env/bin/pip install MySQL-python
    #sudo -u ndnmap pip install MySQL-python
    sudo pip install MySQL-python
}

configure_local_settings() {
    local debug="False"
    if [[ "$BRANCH" =~ "develop" ]]; then
       debug="True"
    fi 
    cat <<EOF | sudo -u $LOCAL_USER tee $PROJECT_ROOT/WebServer/settings_local.py
DEBUG = $debug
TEMPLATE_DEBUG = $debug
SERVE_MEDIA = False

DATABASES = {
    'default': {
       'ENGINE': 'django.db.backends.mysql',
       'NAME': '$DB_NAME',
       'USER': '$DB_USER',
       'PASSWORD': '$DB_PASS',
       'HOST': '$DB_HOST',
    }
}

MEDIA_URL = '$STATIC_URL/'
STATIC_URL = '$STATIC_URL/'
ADMIN_MEDIA_PREFIX = '$STATIC_URL/admin/'
TEMPLATE_DIRS = ('$PROJECT_ROOT/WebServer/templates', )
EOF
}

django_syncdb() {
    sudo -u $LOCAL_USER python $PROJECT_ROOT/WebServer/manage.py syncdb --noinput
    if [ -f "$FIXTURE" ]; then
        # load initial data
        sudo -u $LOCAL_USER python $PROJECT_ROOT/WebServer/manage.py loaddata $FIXTURE
    fi 
}

django_print_info() {
    PUBLIC_DNS=`curl http://169.254.169.254/latest/meta-data/public-hostname 2>/dev/null`
    cat <<EOF
** To view the app you just installed, point your browser to **

http://${PUBLIC_DNS}/

EOF
}

#------------------------------------------------------------

getargs "$@"
setdefaults


#------------------------------------------------------------

if [ $RESTART = "FALSE" ]
then
  echo "JDD: RESTART = FALSE"
  # update system
  sudo apt-get update
  sudo apt-get upgrade -y

  # install MySQL (unattended) with *blank* root password
  #sudo DEBIAN_FRONTEND=noninteractive aptitude install -y mysql-server
  sudo apt-get install -y mysql-server

  sudo service mysql stop

  # restart mysql 
  sudo service mysql start
  echo "*** Done. Mysql is now running ***"

  #------------------------------------------------------------
  #update_system # already done abover
  install_baseline
  install_python
  install_webserver
 
  bootstrap_project
  install_project1
  install_project2
  configure_local_settings

  #mysqladmin -u root password mysql

  create_mysql_database
  load_sql
  #open_external_port
  print_mysql_config

  #------------------------------------------------------------
  

  #------------------------------------------------------------


else
  echo "JDD: RESTART = TRUE"
fi

activate_webserver
  
django_syncdb
django_print_info


echo "JDD: DONE"
#------------------------------------------------------------



