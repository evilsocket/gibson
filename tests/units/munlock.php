<?php 

class Munlock extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'fuu', 'rar' ), 'rar' );

        $this->assertEqual( $this->client->mlock( 'f', 1 ), 2 );

        $this->assertFalse( $this->client->set( 'foo', 'new' ) );
        $this->assertFalse( $this->client->set( 'fuu', 'new' ) );

        $this->assertEqual( $this->client->munlock( 'f' ), 2 );

        $this->assertEqual( $this->client->set( 'foo', 'new' ), 'new' );
        $this->assertEqual( $this->client->set( 'fuu', 'new' ), 'new' );

    }

    public function clean(){
        $this->client->munlock('f');
        $this->client->mdel('f');
    }
}

?>
