# LG Ducted split control via Arduino with MQTT
DISCLAIMER - This is a code implementation and interface for connecting to an LG airconditioner.  This is a 'hack' and does not use any authorised or otherwise non potentially warranty voiding techniques. Having said that, I have been using this system for quite some time and have not run in to any damaging issues but there are some caveats to heed (see below).  This information is provided as-is without any warranty or claim that it will work as expected or not damage your system.  It is simply a recount of the techniques I used to accomplish the task that I am sharing for the greater good. No liability accepted.

## This is not another Infrared remote control emulation hack!
My particular AC has no usable interface designed for any sort of control other than the included wall mounted controls. Unfortunately it was made at a time where IoT was not high on any manufacturers list. I discovered it had some options for 'master' control but even though the unit was only 2 years old at the time of me first attempting this, the expansion boards were unobtanium and prices were astronomical anyway. As was the 'Wireless RF Remote' addon which could have made things a lot easier but impossible to buy.

Had it been my choice, it would not be an LG but since it was installed in the house when I purchased it (and it's replacement cost would likely be in excess of $10k) it's what I had to deal with.

Aim - To be able to control the AC via MQTT for the purposes of automation via OpenHAB and IFTTT/Google Assistant

These controllers are based on a half duplex (single wire) two way serial communication protocol at 104bps, with 12v TTL type logic (no negative voltage).

The code is based on using an Arduino Uno with Wiznet shield.  There is no reason that this could not be done via serial with a Pro mini etc but it will not work with an ESP8266 as is.  The ESP8266 will not allow for serial speeds low enough.  However, it is possible that you could use a pro mini for the AC interface and i2C to an ESP8266 to still make it wireless. I would strongly recommend NOT using the 12v supply from the controller connector to power any of this interface, it is designed for low current and the power supply in the main unit is unlikely able to cope with the current required.

## Caveats

YMMV if you do not have an identical system to mine.

This interface gives you the ability to control the system in a way that is not as intended, i.e it allows you to change the zone dampers, fan speed and mode without the system being on. I have discovered that if you do change the zones when the main power is off, it sets some sort of error where ALL dampers seem to be half open and half closed. The only way to reset this is to power cycle the entire AC system (at the main isolator/fuse).  It's not damaging, just annoying.

This is a single wire bus, used for both Tx and Rx.  There does not seem to be a method that the controllers use to negotiate or otherwise 'time' sending data (or at least I have not found it). The components (wall controllers and main unit) do seem to send updates at regular intervals though and the controllers will error if they do not receive data for a while. The code tries to predict a time window to transmit the data to prevent collisions. It is not perfect and sometimes commands may have to be retried to be successful due to collisions on the bus.

## Connections

The physical RX interface utilises a logic level converter to bring the 12v signal down to something the Arduino is ok with.
For the TX, we simply use an opto-isolator, LED and current limiting resistor to be able to pull the data line low.  It sounds technical but it's really straight forward.
The Signal line from the AC has an extremely weak pullup on it - The current required to pull it low is very small.

The LED also gives you a good indication of when data is being transmitted from the controller (I have not implemented an Rx LED).

<Todo: Arduino Interface Pic>


## Usage

The system is designed to publish and process information posted to a particular MQTT Topic.  Since I have many modules for other things on my system, I use a 4 character module ID, based on the MAC address it is using.
By default, it will use Topics:
ha/mod/5557/<feature> <Value>

Feature|Function|Possible values
-------|--------|---------------
P|Power|0=off/1=on
F|Fan|0=low/1=Med/2=High
M|Mode|0=Cool/1=Dehumidify/2=Fan/3=Auto/4=Heat
Z|Zones|Binary representation of the 4 zones as a string, ie 1011
T|Temp|18-30

If is then up to you to implement these in your home automation system but if you are using OpenHAB like me, then this should help:
```
Items:
Switch AC_Power { mqtt=">[control:ha/mod/5557/P:command:ON:1],>[control:ha/mod/5557/P:command:OFF:0],<[control:ha/mod/5557/P:state:MAP(simpleBinary.map)],<[control:ha/mod/5557/P:state:MAP(simpleBinary.map)]" }
Number AC_Mode  <climate>       { mqtt=">[control:ha/mod/5557/M:command:*:default],<[control:ha/mod/5557/M:state:default]" }
Number AC_Temp  <temperature>   { mqtt=">[control:ha/mod/5557/T:command:*:default],<[control:ha/mod/5557/T:state:default]" }
Number AC_Fan   <flow>          { mqtt=">[control:ha/mod/5557/F:command:*:default],<[control:ha/mod/5557/F:state:default]" }
String AC_Zone  <house>         { mqtt=">[control:ha/mod/5557/Z:command:*:default],<[control:ha/mod/5557/Z:state:default]" }
Switch AC_Zone0
Switch AC_Zone1
Switch AC_Zone2
Switch AC_Zone3


Sitemap:
     Group item="HVAC" label="HVAC" icon="climate-on" {
       Switch item=AC_Power label="Power"
       Selection item=AC_Mode label="Mode" mappings=[0="Cool", 1="DH", 2="Fan", 3="Auto", 4="Heat"]
       Selection item=AC_Fan  label="Fan"  mappings=[0="Low", 1="Med", 3="High"]
       Setpoint item=AC_Temp label="Temp [%d]" minValue=18 maxValue=30 step=1
       Group item="Zones" label="Zones" {
         Switch item=AC_Zone0 label="Zone 1"
         Switch item=AC_Zone1 label="Zone 2"
         Switch item=AC_Zone2 label="Zone 3"
         Switch item=AC_Zone3 label="Zone 4"
       }
     }

Rules:
rule "Aircon Zone Control input"
when Item AC_Zone received update
then
  logInfo("Aircon","AC_Zone got update")
  val String zoneState = AC_Zone.state.toString
  val String zoneState0 = zoneState.substring(0,1)
  val String zoneState1 = zoneState.substring(1,2)
  val String zoneState2 = zoneState.substring(2,3)
  val String zoneState3 = zoneState.substring(3,4)

  if(zoneState != "0000"){
    if(zoneState0 == "1") {
      postUpdate(AC_Zone0, "ON")
    }else{
     postUpdate(AC_Zone0, "OFF")
    }
    if(zoneState1 == "1") {
      postUpdate(AC_Zone1, "ON")
    }else{
      postUpdate(AC_Zone1, "OFF")
    }
    if(zoneState2 == "1") {
      postUpdate(AC_Zone2, "ON")
    }else{
      postUpdate(AC_Zone2, "OFF")
    }
    if(zoneState3 == "1") {
      postUpdate(AC_Zone3, "ON")
    }else{
      postUpdate(AC_Zone3, "OFF")
    }
  }
end

rule "Aircon Zone Control output"
when Item AC_Zone0 received command or
     Item AC_Zone1 received command or
     Item AC_Zone2 received command or
     Item AC_Zone3 received command
then
     val String zoneSwitchState = "0000"
     if (AC_Zone0.state == ON){
       logInfo("Aircon","AC_ZoneX read state")
       zoneSwitchState = '1'
     }else{
       zoneSwitchState = '0'
     }
     if (AC_Zone1.state == ON){
       zoneSwitchState = zoneSwitchState + '1'
     }else{
       zoneSwitchState = zoneSwitchState + '0'
     }
     if (AC_Zone2.state == ON){
       zoneSwitchState = zoneSwitchState + '1'
     }else{
       zoneSwitchState = zoneSwitchState + '0'
     }
     if (AC_Zone3.state == ON){
       zoneSwitchState = zoneSwitchState + '1'
     }else{
       zoneSwitchState = zoneSwitchState + '0'
     }
     if (zoneSwitchState != "0000") {
       AC_Zone.sendCommand(zoneSwitchState)
     }
end
```