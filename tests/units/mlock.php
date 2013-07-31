<?php 

class Mlock extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'fuu', 'rar' ), 'rar' );

        $this->assertEqual( $this->client->mlock( 'f', 1 ), 2 );

        $this->assertFalse( $this->client->set( 'foo', 'new' ) );
        $this->assertFalse( $this->client->set( 'fuu', 'new' ) );

        sleep(1);

        $this->assertEqual( $this->client->set( 'foo', 'new' ), 'new' );
        $this->assertEqual( $this->client->set( 'fuu', 'new' ), 'new' );
    }

    public function testAlreadyLocked(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'fuu', 'rar' ), 'rar' );

        $this->assertTrue( $this->client->lock( 'foo', 10 ) );

        $this->assertEqual( $this->client->mlock( 'f', 1 ), 1 );
    }

    public function clean(){
        $this->client->munlock('f');
        $this->client->mdel('f');
    }
}

?>
