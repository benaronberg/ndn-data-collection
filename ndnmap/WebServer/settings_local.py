DEBUG = False
TEMPLATE_DEBUG = False
SERVE_MEDIA = False

DATABASES = {
    'default': {
       'ENGINE': 'django.db.backends.mysql',
       'NAME': 'ndnmap',
       'USER': 'ndnmap',
       'PASSWORD': '',
       'HOST': 'localhost',
    }
}

MEDIA_URL = 'http://www.arl.wustl.edu/~jdd/NDN/static//'
STATIC_URL = 'http://www.arl.wustl.edu/~jdd/NDN/static//'
ADMIN_MEDIA_PREFIX = 'http://www.arl.wustl.edu/~jdd/NDN/static//admin/'
TEMPLATE_DIRS = ('/home/ndnmap/ndnmap/templates', )
