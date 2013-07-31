<?php 

class Set extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', 'bar', 1 ), 'bar' );

        sleep( 1 );

        $this->assertFalse( $this->client->get( 'foo' ) );
    }

    public function testUTF8(){
        $values = array
        (
            "This is the Euro symbol '€'.",
            "Télécom",
            "Weiß, Goldmann, Göbel, Weiss, Göthe, Goethe und Götz"	
        );

        foreach( $values as $value ){
            $this->assertEqual( $this->client->set( 'foo', $value ), $value );
            $this->assertEqual( $this->client->get( 'foo' ), $value );
        }
    }

    public function testBigBuffer(){
        $big = str_repeat( "a", 1000000 );

        $this->assertEqual( $this->client->set( 'foo', $big ), $big );
        $this->assertEqual( $this->client->get( 'foo' ), $big );
    }

    public function testBinaryKey(){
        for( $i = 0; $i < 100; $i++ ){
            $key = 'foo'.md5( $i,true );
            $key = str_replace( ' ', '_', $key ); // space is used to split key and value
            $val = "value$i";

            $this->assertEqual( $this->client->set( $key, $val ), $val );
            $this->assertEqual( $this->client->get( $key ), $val );
        }

        $this->assertEqual( $this->client->mdel('foo'), 100 );
    }

    public function testBinaryValue(){
        $v = md5( 'gibson', true );
        $this->assertEqual( $this->client->set( 'foo', $v ), $v );
        $this->assertEqual( $this->client->get( 'foo' ), $v );

        $this->assertTrue( $this->client->del( 'foo' ) );
    }

    public function testBinaryBoth(){
        for( $i = 0; $i < 100; $i++ ){
            $key = 'foo'.md5( $i,true );
            $key = str_replace( ' ', '_', $key ); // space is used to split key and value
            $val = md5( "value$i", true );

            $this->assertEqual( $this->client->set( $key, $val ), $val );
            $this->assertEqual( $this->client->get( $key ), $val );
        }

        $this->assertEqual( $this->client->mdel('foo'), 100 );
    }

    public function clean(){
        $this->client->mdel('foo');
    }
}

?>
