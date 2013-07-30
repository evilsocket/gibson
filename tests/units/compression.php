<?php 

class Compression extends BaseUnit
{
    private function generateRandomString($length = 10) {
        $characters = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        $randomString = '';
        for ($i = 0; $i < $length; $i++) {
            $randomString .= $characters[rand(0, strlen($characters) - 1)];
        }
        return $randomString;
    }

    public function test(){
        $big  = str_repeat( "a", 50000 );
        $big2 = str_repeat( "helloworld", 10000 ); 

        $this->assertEqual( $this->client->set( 'big',  $big  ), $big );
        $this->assertEqual( $this->client->set( 'big2', $big2 ), $big2 );

        $stats = $this->client->stats();

        $this->assertIsSet( $stats, 'compr_rate_avg' );
        $this->assertTrue( $stats['compr_rate_avg'] > 0 );

        $this->assertEqual( $this->client->mdel( 'big' ), 2 );
    }

    // Regression test for issue #5:
    // The encoding doesn't get updated when a LZF encoded item is decompressed and sent in a multi set.
    public function testIssue005(){
        $big = str_repeat( "a", 50000 );

        $this->assertEqual( $this->client->set( 'big',  $big  ), $big );
        $this->assertEqual( $this->client->set( 'big2', $big ), $big );

        $multi = $this->client->mget( 'big' );

        $this->assertIsA( $multi );
        $this->assertEqual( count($multi), 2 );

        $this->assertEqual( $multi, array( 'big' => $big, 'big2' => $big ) );
    }

    // Regression test for issue #8:
    // Compression rate average is wrong ( always 0 ).
    public function testIssue008(){
        for( $i = 0; $i < 1000; $i++ ){
            $big = str_repeat( $this->generateRandomString(), rand( 500, 1000 ) );
            
            $this->assertEqual( $this->client->set( "big$i", $big ), $big );
        }

        $stats = $this->client->stats();

        $this->assertIsSet( $stats, 'compr_rate_avg' );
        $this->assertTrue( $stats['compr_rate_avg'] >= 90 );

        $this->assertEqual( $this->client->mdel( 'big' ), 1000 );
    }
    
    public function clean(){
        $this->client->mdel('big');
    }
}

?>
