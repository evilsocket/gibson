<?php 

class Meta extends BaseUnit
{
    // TODO: Other tests
    
    public function testEncoding(){
        $big = str_repeat( "a", 50000 );

        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aab', '1' ), '1' );
        $this->assertEqual( $this->client->set( 'aac', $big ), $big );

        $this->assertEqual( $this->client->inc( 'aab' ), 2 );
        $this->assertEqual( $this->client->meta( 'aaa', 'encoding' ), Gibson::ENC_PLAIN );
        $this->assertEqual( $this->client->meta( 'aab', 'encoding' ), Gibson::ENC_NUMBER );
        $this->assertEqual( $this->client->meta( 'aac', 'encoding' ), Gibson::ENC_LZF ); 
    }

    public function clean(){
        $this->client->mdel('a');
    }
}

?>
