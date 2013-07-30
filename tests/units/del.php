<?php 

class Del extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar' ), 'bar' );
        $this->assertTrue( $this->client->del( 'foo' ) );
        $this->assertFalse( $this->client->del( 'foo' ) );
        $this->assertFalse( $this->client->get( 'foo' ) );
    }

    public function clean(){
        $this->client->del('foo');
    }
}

?>
