<?php 

class Dec extends BaseUnit
{
    public function test(){
        $this->assertEqual( $this->client->set( 'foo', '1' ), '1' );

        for( $i = 0; $i >= -50; $i-- ){
            $this->assertEqual( $this->client->dec( 'foo' ), $i );
            $this->assertEqual( $this->client->get( 'foo' ), $i );
        }
    }

    public function clean(){
        $this->client->del('foo');
    }
}

?>
