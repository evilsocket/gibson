<?php 

class Mdel extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aab', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aac', 'bar' ), 'bar' );

        $this->assertEqual( $this->client->mdel('a'), 3  );

        $this->assertFalse( $this->client->get( 'aaa' ) );
        $this->assertFalse( $this->client->get( 'aab' ) );
        $this->assertFalse( $this->client->get( 'aac' ) );

        $this->assertFalse( $this->client->mdel( 'a' ) );
    }

    public function testBig(){
        $big = str_repeat( "a", 50000 );

        for( $i = 0; $i < 500; $i++ ){
            $this->assertEqual( $this->client->set( "abig$i", $big ), $big );
        }

        $this->assertEqual( $this->client->mdel( 'abig' ), 500 );    
    }

    public function clean(){
        $this->client->mdel('a');
    }
}

?>
