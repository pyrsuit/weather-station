use rumqttc::{MqttOptions, Client, QoS};
use rusqlite::{params, Connection};
use serde::Deserialize;
use serde_json;
use std::time::Duration;
use std::thread;

#[derive(Deserialize)]
struct SensorPayload {
    timestamp: i64,
    metric: String,
    value: f64,
    unit: String,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let conn = Connection::open("weather_station.db").expect("Failed to open DB");
    conn.execute(
        "CREATE TABLE IF NOT EXISTS measurements (
            timestamp TEXT NOT NULL,
            value REAL NOT NULL,
            metric TEXT NOT NULL,
            unit TEXT NOT NULL
        )",
        [],
    )?;      

    let mut mqttoptions = MqttOptions::new("mqtt_logger", "host", 1883);

    mqttoptions.set_keep_alive(Duration::from_secs(15 * 60));

    mqttoptions.set_clean_session(false);

    let (client, mut eventloop) = Client::new(mqttoptions, 10);
    client.subscribe("weather/indoor-sensor/temperature", QoS::AtMostOnce)?;
    client.subscribe("weather/indoor-sensor/humidity", QoS::AtMostOnce)?;

    // Spawn a thread for the connection loop
    thread::spawn(move || {
        for message in eventloop.iter() {
            match message {
                Ok(rumqttc::Event::Incoming(rumqttc::Packet::Publish(p))) => {
                    if let Ok(text) = String::from_utf8(p.payload.to_vec()) {
                        match serde_json::from_str::<SensorPayload>(&text) {
                        Ok(data) => {
                            if let Err(e) = conn.execute(
                                    "INSERT INTO measurements
                                        (timestamp, value, metric, unit)
                                    VALUES (?1, ?2, ?3, ?4)",
                                    params![
                                        data.timestamp,
                                        data.value,
                                        data.metric,
                                        data.unit
                                    ],
                                )  {
                                    eprintln!("DB insert error: {}", e);
                                } else {
                                    println!("Inserted @ {}", data.timestamp);
                                }
                            }
                            Err(e) => eprintln!("JSON parse error: {}", e),
                        }
                    }
                }
                Ok(_) => {}
                Err(e) => {
                    eprintln!("MQTT error: {} — reconnecting in 5s", e);
                    std::thread::sleep(std::time::Duration::from_secs(5));
                }
            }
        }
    });
    // Block main thread forever
    loop {
        thread::sleep(Duration::from_secs(60));
    }
}
