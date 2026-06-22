using Mqtt.Controllers;
using DeviceLocator.Api.Data;
using DeviceLocator.Api.Models;
using DeviceLocator.Api.Mqtt.Payloads;

namespace DeviceLocator.Api.Mqtt.Controllers;

public class DeviceLocationMqttController(
    IServiceScopeFactory scopeFactory,
    ILogger<DeviceLocationMqttController> logger) : MqttController
{
    [MqttRoute("fsiot/devicelocator/telemetry")]
    public async Task OnDeviceLocationTelemetry(DeviceLocationPayload payload)
    {
        try
        {
            using var scope = scopeFactory.CreateScope();
            var db = scope.ServiceProvider.GetRequiredService<AppDbContext>();

            var location = new DeviceLocation
            {
                DeviceName = payload.DeviceName,
                Rssi = payload.Rssi,
                Distance = payload.Distance,
                Status = payload.Status,
                Timestamp = DateTime.UtcNow
            };

            db.DeviceLocations.Add(location);
            await db.SaveChangesAsync();

            logger.LogInformation("Persisted device location from {DeviceName}: RSSI={Rssi}dBm, Distance={Distance}m",
                payload.DeviceName, payload.Rssi, payload.Distance);
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "Error processing device location telemetry for device {DeviceName}", payload.DeviceName);
        }
    }
}
