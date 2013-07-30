<?php 

class Ping extends BaseUnit
{
    public function test(){
        $this->assertTrue( $this->client->ping() );
    }
}

?>
