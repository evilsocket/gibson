<?php

class Server
{
    private $custom_socket  = NULL;
    private $default_config = array
    (
       "logfile"           => "/dev/null",
       "loglevel"          => 0,
       "logflushrate"      => 1,
       "unix_socket"       => "/tmp/gibson.sock",
       "daemonize"         => 1,
       "pidfile"           => "/tmp/gibson.pid",
       "max_memory"        => "1G",
       "max_item_ttl"      => 2592000,
       "max_idletime"      => 5,
       "max_clients"       => 255,
       "max_request_size"  => "10M",
       "max_key_size"      => "512K",
       "max_value_size"    => "9M",
       "max_response_size" => "15M",
       "compression"       => 500,
       "cron_period"       => 100
    );
    public $override = array();


    public function __construct( $custom_socket = NULL ){
        $this->custom_socket = $custom_socket;
    } 

    private function updateConfigFile(){
        $config = '';
        foreach( $this->default_config as $key => $value ){
            if( isset( $this->override[$key] ) && $this->override[$key] !== NULL )
                $config .= "$key {$this->override[$key]}\n";
            else
                $config .= "$key $value\n";
        }   

        file_put_contents( '/tmp/gibson.conf', $config );
    }

    public function start(){
        if( $this->custom_socket == NULL ){
            $this->updateConfigFile();

            system("cd .. && ./gibson -c /tmp/gibson.conf");
        }
    }

    public function stop(){
        if( $this->custom_socket == NULL ){
            system('killall gibson &> /dev/null');
            @unlink( '/tmp/gibson.conf' );
        }
    }

    public function restart(){
        $this->stop();
        $this->start();
    }   

    public function getSocket(){
        return $this->custom_socket ? $this->custom_socket : '/tmp/gibson.sock';
    }
}   

?>
