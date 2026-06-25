const mqttBroker = "mqtt://localhost:1883";
const mqttUsername = "4c";
const mqttPassword = "123123";

const thingsboard = {
  token: "aV4rVeDAlpgINvsb2sHp",
  url: "http://demo.thingsboard.io/api/v1/",
};
const topics = {
  trashbin: "trashbin/status",
};

export default {
  mqttBroker,
  mqttUsername,
  mqttPassword,
  thingsboard,
  topics,
};
