<?php 

class Mttl extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aab', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aac', 'bar' ), 'bar' );

        $this->assertEqual( $this->client->mttl( 'a', 1 ), 3  );

        sleep(1);

        $this->assertFalse( $this->client->get( 'aaa' ) );
        $this->assertFalse( $this->client->get( 'aab' ) );
        $this->assertFalse( $this->client->get( 'aac' ) );
    }

    public function clean(){
        $this->client->mdel('a');
    }
}

?>
