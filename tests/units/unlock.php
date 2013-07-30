<?php 

class Unlock extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertTrue( $this->client->lock( 'foo', 1 ) );

        $this->assertFalse( $this->client->set( 'foo', 'new' ) );

        $this->assertTrue( $this->client->unlock( 'foo' ) );

        $this->assertTrue( $this->client->set( 'foo', 'new' ) );
    }

    public function clean(){
        $this->client->unlock('foo');
        $this->client->del('foo');
    }
}

?>
