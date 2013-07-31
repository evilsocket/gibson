<?php

class Server
{
    private $custom_socket = NULL;

    public function __construct( $custom_socket = NULL ){
        $this->custom_socket = $custom_socket;
    } 

    public function start(){
        if( $this->custom_socket == NULL ){
            $config = 
"logfile      /dev/null
loglevel     0
logflushrate 1
unix_socket  /tmp/gibson.sock
daemonize 1
pidfile   /tmp/gibson.pid
max_memory       1G
max_item_ttl     2592000
max_idletime     5
max_clients      255
max_request_size 10M
max_key_size 512K
max_value_size 9M
max_response_size 15M
compression 500
cron_period      100";
            
            file_put_contents( '/tmp/gibson.conf', $config );

            system("cd .. && ./gibson -c /tmp/gibson.conf");
        }
    }

    public function stop(){
        if( $this->custom_socket == NULL ){
            system('killall gibson &> /dev/null');
            @unlink( '/tmp/gibson.conf' );
        }
    }

    public function getSocket(){
        return $this->custom_socket ? $this->custom_socket : '/tmp/gibson.sock';
    }
}   

?>
