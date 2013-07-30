<?php 

class Mset extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'fuu', 'rar' ), 'rar' );

        $this->assertEqual( $this->client->mset( 'f', 'new' ), 2 );

        $this->assertEqual( $this->client->get( 'foo' ), 'new' );
        $this->assertEqual( $this->client->get( 'fuu' ), 'new' );

    }

    public function clean(){
        $this->client->mdel('f');
    }
}

?>
