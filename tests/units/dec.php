<?php 

class Dec extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', '1' ), '1' );
        $this->assertEqual( $this->client->dec( 'foo' ), 0 );
        $this->assertEqual( $this->client->get( 'foo' ), 0 );
    }

    public function clean(){
        $this->client->del('foo');
    }
}

?>
