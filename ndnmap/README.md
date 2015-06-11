ndnmap web app
==============


This web app displays a geographic map of the NDN testbed with real-time 
bandwidth data.

There are two parts to the app, the WebServer and the DataCollection.

For the GEC-13 demo in March we ran the ndnmap web server on an AWS EC2 instance.
There may still be some lingering vestiges of that implementation in this release.
Sorry if that causes any confusion.
 

Installation
------------
    
Clone `ndnmap`.

   > git clone https://WU-ARL@github.com/WU-ARL/ndnmap.git

   > cd ndnmap/WebServer

   > ./install.sh -D ndnmap -n ndnmap -U ndnmap -w example.com -s http://www.arl.wustl.edu/~jdd/NDN/static -P <mysql-password> -g https://WU-ARL@github.com/WU-ARL/ndnmap.git -u ndnmap -p /home/ndnmap/ndnmap -f /home/ndnmap/ndnmap/WebServer/deploy/initial_data.json

   # and add your Google Maps Key in place of XXX on the line with GMAP_API_KEY='XXX'

   > sudo vi /home/ndnmap/ndnmap/WebServer/settings.py    

   # restart the web server 

   > sudo /etc/init.d/apache2 reload

-------------------------------------
Here are the old install directions:
Install the packages in ``deploy/requirements.txt``.

  * ``pip`` (and optionally [virtualenv][1]) simplifies the process

        sudo easy_install -U pip # requires setuptools
        pip install -r deploy/requirements.txt
        

-------------------------------------

Obtain a [key for the Google maps API][2] and enter the API key in
`ndnmap/settings.py`:

    # API key for the Google Maps applications
    GMAP_API_KEY='XXX'

Local Server
----------------

In `ndnmap` dir, run
    
    ./manage.py runserver
    
Open the home page in your browser: http://localhost:8000/


Conventions
-----------

The routers report transmit (tx) and receive (rx) bits via an HTTP request:

    example.com/bw/<link_id>/<time>/<rx_bits>/<tx_bytes>/

Field descriptions

  * `link_id` (integer) link identifier 
  * `time` (interger) timestamp (seconds since the epoch)
  * `rx_bits` (integer) number of bits received (monotonically increasing)
  * `tx_bits` (integer) number of bits transmitted (monotonically increasing)
  
Settings
--------
Other relevant but optional settings in `ndnmap/settings.py` follow.

    # Show 0 bandwidth if time since last update > GMAP_LINK_ALIVE_INTERVAL (s)
    GMAP_LINK_ALIVE_INTERVAL = 10 
    
    # Update bandwidth on map every GMAP_BW_UPDATE_INTERVAL (s)
    GMAP_BW_UPDATE_INTERVAL = 0.5
    
Testing 
-------

### Unittest

Test the `gmap` application.

    fab test

Generate fake data for visual tests:

  1.  Use the admin site: example.com/admin (`user=demo and pass=demo`)
  2.  Run the `xb.sh` scripts: `ndnmap/gmap/test.xb -h `


### Reset database

Remove all `gmap` records from the database

    fab reset

[1]: http://mathematism.com/2009/07/30/presentation-pip-and-virtualenv/
[2]: https://developers.google.com/maps/documentation/javascript/tutorial#api_key
[3]: http://console.aws.amazon.com/
[4]: http://fabfile.org/
[5]: http://alestic.com/2010/10/ec2-ssh-keys
