<?php 

class Inc extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', '1' ), '1' );
        $this->assertEqual( $this->client->inc( 'foo' ), 2 );
        $this->assertEqual( $this->client->get( 'foo' ), 2 );
    }

    public function testNaN(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );

        $this->assertFalse( $this->client->inc('foo') );

        $this->assertEqual( $this->client->get('foo'), 'bar' );
    }

    public function clean(){
        $this->client->del('foo');
    }
}

?>
