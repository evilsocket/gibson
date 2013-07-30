<?php 

class Ttl extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertTrue( $this->client->ttl( 'foo', 1 ) );

        sleep(1);

        $this->assertFalse( $this->client->get( 'foo' ) );
    }

    public function clean(){
        $this->client->del('foo');
    }
}

?>
