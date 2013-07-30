<?php 

class Lock extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertTrue( $this->client->lock( 'foo', 1 ) );

        $this->assertFalse( $this->client->set( 'foo', 'new' ) );

        sleep(1);

        $this->assertTrue( $this->client->set( 'foo', 'new' ) );
        $this->assertTrue( $this->client->del('foo') );
    }

    public function testDoubleLock(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertTrue( $this->client->lock( 'foo', 10 ) );
        $this->assertFalse( $this->client->lock( 'foo', 10 ) );

        $this->assertFalse( $this->client->del('foo') );
        $this->assertTrue( $this->client->unlock('foo') );
        $this->assertTrue( $this->client->del('foo') );
    }

    public function clean(){
        $this->client->unlock('foo');
        $this->client->del('foo');
    }
}

?>
