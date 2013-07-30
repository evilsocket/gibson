<?php 

class Get extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->get( 'foo' ), 'bar' );
    }

    public function clean(){
        $this->client->del('foo');
    }
}

?>
