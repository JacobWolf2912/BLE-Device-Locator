using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using DeviceLocator.Api.Data;
using DeviceLocator.Api.Models;

namespace DeviceLocator.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class LocationController(AppDbContext context) : ControllerBase
{
    [HttpGet("latest")]
    public async Task<ActionResult<IEnumerable<DeviceLocation>>> GetLatestLocations()
    {
        var latestLocations = await context.DeviceLocations
            .OrderByDescending(d => d.Timestamp)
            .Take(100)
            .ToListAsync();

        var result = latestLocations
            .GroupBy(d => d.DeviceName)
            .Select(g => g.First())
            .OrderByDescending(d => d.Timestamp)
            .ToList();

        return Ok(result);
    }

    [HttpGet("device/{deviceName}")]
    public async Task<ActionResult<IEnumerable<DeviceLocation>>> GetDeviceHistory(string deviceName, [FromQuery] int? hours = 24)
    {
        var startTime = DateTime.UtcNow.AddHours(-(hours ?? 24));

        var locations = await context.DeviceLocations
            .Where(d => d.DeviceName == deviceName && d.Timestamp >= startTime)
            .OrderByDescending(d => d.Timestamp)
            .ToListAsync();

        return Ok(locations);
    }

    [HttpGet("all")]
    public async Task<ActionResult<IEnumerable<DeviceLocation>>> GetAllLocations([FromQuery] int? limit = 100)
    {
        var locations = await context.DeviceLocations
            .OrderByDescending(d => d.Timestamp)
            .Take(limit ?? 100)
            .ToListAsync();

        return Ok(locations);
    }

    [HttpGet("devices")]
    public async Task<ActionResult<IEnumerable<object>>> GetDevices()
    {
        var devices = await context.DeviceLocations
            .GroupBy(d => d.DeviceName)
            .Select(g => new
            {
                id = g.Key,
                name = g.Key,
                location = "BLE Paired Device",
                latestStatus = g.OrderByDescending(d => d.Timestamp).First().Status,
                latestTimestamp = g.OrderByDescending(d => d.Timestamp).First().Timestamp
            })
            .OrderByDescending(d => d.latestTimestamp)
            .ToListAsync();

        return Ok(devices);
    }

    [HttpPost("command/find")]
    public async Task<ActionResult> SendFindCommand()
    {
        var logger = HttpContext.RequestServices.GetRequiredService<ILogger<LocationController>>();

        try
        {
            logger.LogInformation("Find device command received");

            // Publish find command to MQTT
            var mqttClient = HttpContext.RequestServices.GetRequiredService<HiveMQtt.Client.HiveMQClient>();
            await mqttClient.PublishAsync("fsiot/devicelocator/command", "find");

            logger.LogInformation("Published find command to MQTT topic");

            return Ok(new { message = "Find device command sent" });
        }
        catch (Exception ex)
        {
            logger.LogError(ex, "Error sending find command");
            return BadRequest(new { error = ex.Message });
        }
    }
}
