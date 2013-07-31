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

    public function testLocked(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'fuu', 'rar' ), 'rar' );

        $this->assertTrue( $this->client->lock( 'foo', 10 ) );

        $this->assertEqual( $this->client->mset( 'f', 'new' ), 1 );

    }   

    public function clean(){
        $this->client->munlock('f');
        $this->client->mdel('f');
    }
}

?>
