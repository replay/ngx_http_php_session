Nginx PHP session
-------

This is an Nginx module that can extract values out of a serialized PHP Session and store them into an Nginx variable to make them reusable in the Nginx configuration

Some more explanation
=====================
http://mauro-stettler.blogspot.com/2010/02/nginx-module-to-extract-values-out-of.html

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


Array element specification syntax
==================================

This is a little hard to explain. First, the PHP session serialization is - for some mysterious reason - using two different characters for element separation, ; semicolon and | pipe. I will give an example:

if you store a multidimensional array in $_SESSION['symfony/user/sfUser/attributes'] the serialization of it might look like this:

    symfony/user/sfUser/attributes|s:10:"subscriber";s:9:"getGender"

The type of the first element in a serialized PHP session is not defined, its always string. Thats why its type is also not defined in its serialized representation, while the type for other array keys is. The first level array element in the example has the key "symfony/user/sfUser/attributes". This element stores another, regular array which has a key of type string, key length 10 and value "subscriber". This is defined via 's:10:"subscriber"'. Then there is another sub array with a key of type string and length 9 and content "getGender", like this 's:9:"getGender"'. The best way to find the right notation to to specify a key in an array is to print it out as serialized and then read the serialized string that got generated by PHP. 
The above specified array path would find its way through a session array for example like this:

    $_SESSION['symfony/user/sfUser/attributes'] => Array
        (
            [users_dynamic] => Array
                (
                    [get_last_online_state] => 
                    [update_counter_time] => 1266041164
                )
            [subscriber] => Array
                (
                    [user_actual_culture] => de
                    [lastURI] => http://dev.poppen.lab/frontend_dev.php/home
                    [invisibility] => 0
                    [getGender] => m
                )
        )
