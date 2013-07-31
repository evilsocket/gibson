<?php

class Keys extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aab', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aac', 'bar' ), 'bar' );
    
        $keys = $this->client->keys('a');

        $this->assertIsA( $keys );
        $this->assertEqual( $keys, array( 'aaa', 'aab', 'aac' ) );

        $this->assertTrue( $this->client->del('aab') );

        $keys = $this->client->keys('a');

        $this->assertIsA( $keys );
        $this->assertEqual( $keys, array( 'aaa', 'aac' ) );

        $this->assertTrue( $this->client->del('aaa') );

        $keys = $this->client->keys('a');

        $this->assertIsA( $keys );
        $this->assertEqual( $keys, array( 'aac' ) );
    
        $this->assertTrue( $this->client->del('aac') );

        $this->assertFalse( $this->client->keys('a') );
    }

    public function clean(){
        $this->client->mdel('a');
    }
}

?>
