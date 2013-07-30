<?php 

class Count extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aab', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aac', 'bar' ), 'bar' );

        $this->assertEqual( $this->client->count('a'), 3  );

        $this->assertEqual( $this->client->mdel( 'a' ), 3 );
    }

    public function clean(){
        $this->client->mdel('a');
    }
}

?>
