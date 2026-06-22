using Microsoft.EntityFrameworkCore;
using DeviceLocator.Api.Models;

namespace DeviceLocator.Api.Data;

public class AppDbContext : DbContext
{
    public AppDbContext(DbContextOptions<AppDbContext> options) : base(options) { }

    public DbSet<User> Users { get; set; } = null!;
    public DbSet<DeviceLocation> DeviceLocations { get; set; } = null!;

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        base.OnModelCreating(modelBuilder);

        // User configuration
        modelBuilder.Entity<User>(entity =>
        {
            entity.HasKey(e => e.Id);
            entity.Property(e => e.Username).IsRequired().HasMaxLength(100);
            entity.Property(e => e.PasswordHash).IsRequired();
            entity.HasIndex(e => e.Username).IsUnique();
        });

        // DeviceLocation configuration
        modelBuilder.Entity<DeviceLocation>(entity =>
        {
            entity.ToTable("DeviceLocations");
            entity.HasKey(e => e.Id);
            entity.Property(e => e.DeviceName).IsRequired().HasMaxLength(100);
            entity.Property(e => e.Timestamp)
                .HasDefaultValueSql("CURRENT_TIMESTAMP")
                .ValueGeneratedOnAdd();
            entity.Property(e => e.Rssi).IsRequired();
            entity.Property(e => e.Distance).IsRequired();
            entity.Property(e => e.Status).HasMaxLength(50);

            // Indices for time-series query performance
            entity.HasIndex(e => e.Timestamp);
            entity.HasIndex(e => new { e.DeviceName, e.Timestamp });
        });
    }
}
