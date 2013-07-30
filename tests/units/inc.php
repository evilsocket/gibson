<?php 

class Inc extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', '1' ), '1' );
        $this->assertEqual( $this->client->inc( 'foo' ), 2 );
        $this->assertEqual( $this->client->get( 'foo' ), 2 );
    }

    public function clean(){
        $this->client->del('foo');
    }
}

?>
