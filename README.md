Nginx PHP session
-------

This is an Nginx module that can extract values out of a serialized PHP Session and store them into an Nginx variable to make them reusable in the Nginx configuration

Example
===============

    location / {
        eval $session {
             set $memcached_key "d41d8cd98f00b204e9800998ecf8427e:5b9hm9rspbgajg22eqpqs6ang1";
              memcached_pass 192.168.1.237:11210;
        }

        php_session_parse $result $session "symfony/user/sfUser/attributes|s:10:\"subscriber\";s:9:\"getGender\"";

        if ($result = "s:1:\"w\"")
        {
            # do something
        }

        if ($result = "s:1:\"m\"")
        {
            # do something else 
        }
          
        default_type  text/plain;
    }

