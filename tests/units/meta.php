<?php 

class Meta extends BaseUnit
{
    public function testSize(){
        $big = str_repeat( "a", 50000 );

        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->set( 'aab', '1' ), '1' );
        $this->assertEqual( $this->client->set( 'aac', $big ), $big );
        
        $this->assertEqual( $this->client->meta( 'aaa', 'size' ), 3 );
        $this->assertEqual( $this->client->meta( 'aab', 'size' ), 1 );
        $this->assertTrue( $this->client->meta( 'aac', 'size' ) < 50000 ); 
    }       

    public function testAccess(){
        $now = time();

        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertBetween( $this->client->meta( 'aaa', 'access' ), $now - 1, $now + 1 );
    }

    public function testCreated(){
        $now = time();

        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertBetween( $this->client->meta( 'aaa', 'created' ), $now - 1, $now + 1 );
    }

    public function testTTL(){
        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->meta( 'aaa', 'ttl' ), -1 );
        $this->assertTrue( $this->client->ttl( 'aaa', 10 ) );
        $this->assertEqual( $this->client->meta( 'aaa', 'ttl' ), 10 );
    }

    public function testLeft(){
        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->meta( 'aaa', 'left' ), -1 );
        $this->assertTrue( $this->client->ttl( 'aaa', 10 ) );
        $this->assertEqual( $this->client->meta( 'aaa', 'left' ), 10 );

        sleep(1);

        $this->assertEqual( $this->client->meta( 'aaa', 'left' ), 9 );
    }

    public function testLock(){
        $this->assertEqual( $this->client->set( 'aaa', 'bar' ), 'bar' );
        $this->assertEqual( $this->client->meta( 'aaa', 'lock' ), 0 );
        $this->assertTrue( $this->client->lock( 'aaa', 10 ) );
        $this->assertEqual( $this->client->meta( 'aaa', 'lock' ), 10 );
    }

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
        $this->client->unlock('aaa');
        $this->client->mdel('a');
    }
}

?>
